#define MODULE_LOG_PREFIX "webif"
#include "tcmg-globals.h"
#include "tcmg-crypto.h"
#include "tcmg-log.h"

#ifndef TCMG_OS_WINDOWS
#  include <netdb.h>
#  include <sys/select.h>
#endif

#include "tcmg-webif-internal.h"

/* = Session state = */
s_session       s_sessions[WEB_MAX_SESSIONS];
pthread_mutex_t s_sess_lock = PTHREAD_MUTEX_INITIALIZER;

/* = Server state = */
pthread_t  s_webif_tid;
int8_t     s_webif_running = 0;
int        s_webif_sock    = -1;

void session_gen_token(char *out)
{
	uint8_t rnd[16];
	csprng(rnd, sizeof(rnd));
	for (int i = 0; i < 16; i++)
		snprintf(out + i*2, 3, "%02x", rnd[i]);
	out[WEB_SESSION_LEN] = '\0';
}

void session_create(char *token_out)
{
	session_gen_token(token_out);
	time_t now = time(NULL);
	pthread_mutex_lock(&s_sess_lock);
	int slot = 0;
	time_t oldest = s_sessions[0].expires;
	for (int i = 0; i < WEB_MAX_SESSIONS; i++) {
		if (s_sessions[i].expires <= now) { slot = i; break; }
		if (s_sessions[i].expires < oldest) { oldest = s_sessions[i].expires; slot = i; }
	}
	tcmg_strlcpy(s_sessions[slot].token, token_out, WEB_SESSION_LEN + 1);
	s_sessions[slot].expires = now + WEB_SESSION_TIMEOUT;
	pthread_mutex_unlock(&s_sess_lock);
}

int session_check(const char *token)
{
	if (!token || strlen(token) != WEB_SESSION_LEN) return 0;
	time_t now = time(NULL);
	int ok = 0;
	pthread_mutex_lock(&s_sess_lock);
	for (int i = 0; i < WEB_MAX_SESSIONS; i++) {
		if (s_sessions[i].expires > now &&
		    strncmp(s_sessions[i].token, token, WEB_SESSION_LEN) == 0) {
			s_sessions[i].expires = now + WEB_SESSION_TIMEOUT;
			ok = 1; break;
		}
	}
	pthread_mutex_unlock(&s_sess_lock);
	return ok;
}

const char *cookie_get_session(const char *cookie_hdr, char *buf, int bufsz)
{
	if (!cookie_hdr) return NULL;
	const char *key = "tcmg_session=";
	const char *p = strstr(cookie_hdr, key);
	if (!p) return NULL;
	p += strlen(key);
	int i = 0;
	while (p[i] && p[i] != ';' && p[i] != '\r' && p[i] != '\n' && i < bufsz - 1)
	{
		buf[i] = p[i];
		i++;
	}
	buf[i] = '\0';
	return buf;
}


int buf_printf(char **dst, int *dstsz, int pos, const char *fmt, ...)
{
	va_list ap;
	int needed;
	va_start(ap, fmt);
	needed = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);
	if (pos + needed + 1 >= *dstsz)
	{
		int newsz = pos + needed + 8192;
		char *nb  = (char *)realloc(*dst, newsz);
		if (!nb) return pos;
		*dst   = nb;
		*dstsz = newsz;
	}
	va_start(ap, fmt);
	vsnprintf(*dst + pos, *dstsz - pos, fmt, ap);
	va_end(ap);
	return pos + needed;
}

void url_decode(char *s)
{
	char *r = s, *w = s;
	while (*r)
	{
		if (*r == '%' && r[1] && r[2])
		{
			char h[3] = { r[1], r[2], 0 };
			*w++ = (char)strtol(h, NULL, 16);
			r += 3;
		}
		else if (*r == '+') { *w++ = ' '; r++; }
		else                { *w++ = *r++; }
	}
	*w = '\0';
}

void get_param(const char *qs, const char *key, char *out, int outsz)
{
	out[0] = '\0';
	if (!qs) return;
	int klen = (int)strlen(key);
	const char *p = qs;
	while (*p)
	{
		if (strncmp(p, key, klen) == 0 && p[klen] == '=')
		{
			p += klen + 1;
			int i = 0;
			while (*p && *p != '&' && i < outsz - 1)
				out[i++] = *p++;
			out[i] = '\0';
			url_decode(out);
			return;
		}
		while (*p && *p != '&') p++;
		if (*p == '&') p++;
	}
}

void form_get(const char *body, const char *key, char *out, int outsz)
{
	out[0] = '\0';
	if (!body) return;
	char needle[64];
	snprintf(needle, sizeof(needle), "%s=", key);
	const char *p = strstr(body, needle);
	if (!p) return;
	p += strlen(needle);
	int i = 0;
	while (*p && *p != '&' && i < outsz - 1)
	{
		if (*p == '+') { out[i++] = ' '; p++; }
		else if (*p == '%' && p[1] && p[2])
		{
			char hex[3] = { p[1], p[2], 0 };
			out[i++] = (char)strtol(hex, NULL, 16);
			p += 3;
		}
		else out[i++] = *p++;
	}
	out[i] = '\0';
}

void send_headers_ex(int fd, int code, const char *reason,
                             const char *ctype, int length,
                             const char *set_cookie)
{
	char hdr[768];
	time_t now = time(NULL);
	char date_str[64];
	struct tm tm_s;
	gmtime_r(&now, &tm_s);
	strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", &tm_s);
	char cookie_line[256] = "";
	if (set_cookie && set_cookie[0])
		snprintf(cookie_line, sizeof(cookie_line),
		         "Set-Cookie: tcmg_session=%s; Path=/; HttpOnly; SameSite=Strict\r\n",
		         set_cookie);
	snprintf(hdr, sizeof(hdr),
	         "HTTP/1.1 %d %s\r\n"
	         "Server: %s\r\n"
	         "Date: %s\r\n"
	         "Content-Type: %s\r\n"
	         "Content-Length: %d\r\n"
	         "Cache-Control: no-store, no-cache\r\n"
	         "%s"
	         "Connection: close\r\n"
	         "\r\n",
	         code, reason,
	         WEB_SERVER_NAME,
	         date_str,
	         ctype,
	         length,
	         cookie_line);
	send(fd, SO_CAST(hdr), (int)strlen(hdr), MSG_NOSIGNAL);
}

void send_response_ex(int fd, int code, const char *reason,
                              const char *ctype, const char *body, int blen,
                              const char *set_cookie)
{
	send_headers_ex(fd, code, reason, ctype, blen, set_cookie);
	if (body && blen > 0)
		send(fd, SO_CAST(body), blen, MSG_NOSIGNAL);
}

void send_response(int fd, int code, const char *reason,
                            const char *ctype, const char *body, int blen)
{
	send_response_ex(fd, code, reason, ctype, body, blen, NULL);
}

void send_redirect(int fd, const char *location)
{
	char hdr[256];
	snprintf(hdr, sizeof(hdr),
	         "HTTP/1.1 302 Found\r\nLocation: %s\r\nContent-Length: 0\r\n"
	         "Connection: close\r\n\r\n", location);
	send(fd, SO_CAST(hdr), (int)strlen(hdr), MSG_NOSIGNAL);
}

void send_redirect_with_cookie(int fd, const char *location, const char *token)
{
	char hdr[512];
	snprintf(hdr, sizeof(hdr),
	         "HTTP/1.1 302 Found\r\nLocation: %s\r\n"
	         "Set-Cookie: tcmg_session=%s; Path=/; HttpOnly; SameSite=Strict\r\n"
	         "Content-Length: 0\r\nConnection: close\r\n\r\n",
	         location, token);
	send(fd, SO_CAST(hdr), (int)strlen(hdr), MSG_NOSIGNAL);
}

/* = base64 = */
static const char B64[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void b64_encode(const char *in, int ilen, char *out, int outsz)
{
	int i = 0, o = 0;
	uint8_t *s = (uint8_t *)in;
	while (i < ilen && o + 4 < outsz)
	{
		int rem = ilen - i;
		uint32_t v = ((uint32_t)s[i] << 16)
		           | (rem > 1 ? (uint32_t)s[i+1] << 8 : 0)
		           | (rem > 2 ? (uint32_t)s[i+2]      : 0);
		out[o++] = B64[(v >> 18) & 0x3F];
		out[o++] = B64[(v >> 12) & 0x3F];
		out[o++] = rem > 1 ? B64[(v >> 6) & 0x3F] : '=';
		out[o++] = rem > 2 ? B64[v & 0x3F]        : '=';
		i += 3;
	}
	out[o] = '\0';
}

int check_auth(const char *auth_header)
{
	if (!g_cfg.webif_user[0] && !g_cfg.webif_pass[0]) return 1;
	if (!auth_header) return 0;
	const char *p = strstr(auth_header, "Basic ");
	if (!p) return 0;
	p += 6;
	char got[512];
	int  glen = 0;
	while (*p && *p != '\r' && *p != '\n' && *p != ' ' && glen < (int)sizeof(got) - 1)
		got[glen++] = *p++;
	got[glen] = '\0';
	char creds[256];
	snprintf(creds, sizeof(creds), "%s:%s", g_cfg.webif_user, g_cfg.webif_pass);
	char expected[512];
	b64_encode(creds, (int)strlen(creds), expected, sizeof(expected));
	return ct_streq(got, expected) ? 1 : 0;
}

