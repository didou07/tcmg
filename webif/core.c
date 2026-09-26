#define MODULE_LOG_PREFIX "webif"
#include "../src/core/runtime_state.h"
#include "../src/core/utils.h"
#include "../src/crypto/crypto.h"
#include "../src/log/log.h"
#include "internal/proto.h"
#include "service/service.h"
#include "assets/webif_assets.h"

#ifndef TCMG_OS_WINDOWS
#include <sys/resource.h>
#include <limits.h>
#endif

s_session       s_sessions[WEB_MAX_SESSIONS];
pthread_mutex_t s_sess_lock = PTHREAD_MUTEX_INITIALIZER;

void session_gen_token(char *out)
{
	uint8_t rnd[16];
	csprng(rnd, sizeof(rnd));
	for (int i = 0; i < 16; i++)
		snprintf(out + i * 2, 3, "%02x", rnd[i]);
	out[WEB_SESSION_LEN] = '\0';
}

void session_create(char *token_out)
{
	session_gen_token(token_out);
	time_t now = time(NULL);
	pthread_mutex_lock(&s_sess_lock);
	int    slot   = 0;
	time_t oldest = s_sessions[0].expires;
	for (int i = 0; i < WEB_MAX_SESSIONS; i++) {
		if (s_sessions[i].expires <= now) { slot = i; break; }
		if (s_sessions[i].expires < oldest) { oldest = s_sessions[i].expires; slot = i; }
	}
	tcmg_strlcpy(s_sessions[slot].token, token_out, WEB_SESSION_LEN + 1);
	s_sessions[slot].expires   = now + WEB_SESSION_TIMEOUT;
	s_sessions[slot].issued_at = now;
	pthread_mutex_unlock(&s_sess_lock);
}

int session_check(const char *token)
{
	if (!token || strlen(token) != WEB_SESSION_LEN) return 0;
	time_t now = time(NULL);
	int    ok  = 0;
	pthread_mutex_lock(&s_sess_lock);
	for (int i = 0; i < WEB_MAX_SESSIONS; i++) {
		if (s_sessions[i].expires <= now) continue;
		if (!ct_streq(s_sessions[i].token, token)) continue;
		if (now - s_sessions[i].issued_at > WEB_SESSION_MAX_AGE) break;
		s_sessions[i].expires = now + WEB_SESSION_TIMEOUT;
		ok = 1;
		break;
	}
	pthread_mutex_unlock(&s_sess_lock);
	return ok;
}

const char *cookie_get_session(const char *cookie_hdr, char *buf, int bufsz)
{
	if (!cookie_hdr) return NULL;
	const char *key = "tcmg_session=";
	const char *p   = strstr(cookie_hdr, key);
	if (!p) return NULL;
	p += strlen(key);
	int i = 0;
	while (i < bufsz - 1 && p[i] && p[i] != ';' && p[i] != '\r' && p[i] != '\n') {
		buf[i] = p[i];
		i++;
	}
	buf[i] = '\0';
	return i == WEB_SESSION_LEN ? buf : NULL;
}

void session_invalidate(const char *token)
{
	if (!token || !*token) return;
	pthread_mutex_lock(&s_sess_lock);
	for (int i = 0; i < WEB_MAX_SESSIONS; i++) {
		if (ct_streq(s_sessions[i].token, token)) {
			memset(s_sessions[i].token, 0, WEB_SESSION_LEN + 1);
			s_sessions[i].expires   = 0;
			s_sessions[i].issued_at = 0;
			break;
		}
	}
	pthread_mutex_unlock(&s_sess_lock);
}

static const char B64[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void b64_encode(const char *in, int ilen, char *out, int outsz)
{
	const uint8_t *s = (const uint8_t *)in;
	int i = 0, o = 0;
	while (i < ilen && o + 4 < outsz) {
		int      rem = ilen - i;
		uint32_t v   = ((uint32_t)s[i] << 16)
		             | (rem > 1 ? (uint32_t)s[i+1] << 8 : 0)
		             | (rem > 2 ? (uint32_t)s[i+2]      : 0);
		out[o++] = B64[(v >> 18) & 0x3F];
		out[o++] = B64[(v >> 12) & 0x3F];
		out[o++] = rem > 1 ? B64[(v >>  6) & 0x3F] : '=';
		out[o++] = rem > 2 ? B64[ v        & 0x3F] : '=';
		i += 3;
	}
	out[o] = '\0';
}

int check_auth(const char *auth_header)
{
	return webif_auth_basic_valid(auth_header) ? 1 : 0;
}

int check_credentials(const char *user, const char *pass)
{
	return webif_credentials_valid(user, pass) ? 1 : 0;
}

static int hexval(int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

void url_decode(char *s)
{
	char *r = s, *w = s;
	while (*r) {
		if (*r == '%' && hexval(r[1]) >= 0 && hexval(r[2]) >= 0) {
			int v = hexval(r[1]) * 16 + hexval(r[2]);
			r += 3;
			if (v != 0) *w++ = (char)v;
		} else if (*r == '+') { *w++ = ' '; r++; }
		else                  { *w++ = *r++; }
	}
	*w = '\0';
}

static int web_ncaseeq(const char *a, const char *b, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		unsigned char x = (unsigned char)a[i], y = (unsigned char)b[i];
		if (x >= 'A' && x <= 'Z') x = (unsigned char)(x + 32);
		if (y >= 'A' && y <= 'Z') y = (unsigned char)(y + 32);
		if (x != y) return 0;
		if (!x) return 1;
	}
	return 1;
}

const char *web_header_get(const char *raw, const char *name, char *buf, int bufsz)
{
	buf[0] = '\0';
	size_t      nl  = strlen(name);
	const char *end = strstr(raw, "\r\n\r\n");
	const char *p   = strchr(raw, '\n');
	while (p) {
		p++;
		if (*p == '\r' || *p == '\n' || *p == '\0') break;
		if (end && p > end) break;
		if (web_ncaseeq(p, name, nl) && p[nl] == ':') {
			p += nl + 1;
			while (*p == ' ' || *p == '\t') p++;
			int i = 0;
			while (i < bufsz - 1 && *p && *p != '\r' && *p != '\n') buf[i++] = *p++;
			buf[i] = '\0';
			return buf;
		}
		p = strchr(p, '\n');
	}
	return NULL;
}

static int send_all(int fd, const char *p, int n)
{
	while (n > 0) {
		int k = (int)send(fd, SO_CAST(p), n, MSG_NOSIGNAL);
		if (k < 0) { if (errno == EINTR) continue; return -1; }
		if (k == 0) return -1;
		p += k; n -= k;
	}
	return 0;
}

int req_parse(s_http_req *req, int fd, char *raw, int rawlen)
{
	memset(req, 0, sizeof(*req));
	char uri[512] = {0};
	if (sscanf(raw, "%7s %511s", req->method, uri) < 2)
		return 0;
	if (strlen(uri) >= sizeof(uri) - 1) { req->status = 414; return 1; }

	tcmg_strlcpy(req->path, uri, sizeof(req->path));
	char *q = strchr(req->path, '?');
	if (q) { *q = '\0'; tcmg_strlcpy(req->qs, q + 1, sizeof(req->qs)); }

	if (strcmp(req->method, "POST") == 0) {
		const char *bs = strstr(raw, "\r\n\r\n");
		if (!bs) return 1;
		bs += 4;
		int have = (int)(raw + rawlen - bs);
		if (have < 0) have = 0;

		long clen = 0;
		int has_clen = 0;
		char clbuf[32];
		if (web_header_get(raw, "Content-Length", clbuf, sizeof(clbuf))) {
			has_clen = 1;
			char *e = NULL;
			clen = strtol(clbuf, &e, 10);
			if (e == clbuf || *e != '\0' || clen < 0) { req->status = 400; return 1; }
		}

		if (has_clen && have > clen) {
			req->status = 400;
			return 1;
		}
		if (clen > WEB_POST_MAX || have > WEB_POST_MAX) {

			long discard = clen - have;
			char sink[4096];
			while (discard > 0) {
				int want = discard > (long)sizeof(sink) ? (int)sizeof(sink) : (int)discard;
				int n = (int)recv(fd, RECV_CAST(sink), (size_t)want, 0);
				if (n <= 0) break;
				discard -= n;
			}
			req->status = 413;
			return 1;
		}

		int cap = clen > have ? (int)clen : have;
		req->body = (char *)malloc((size_t)cap + 1);
		if (!req->body) { req->status = 503; return 1; }
		memcpy(req->body, bs, (size_t)have);
		req->body_len = have;
		while (req->body_len < clen) {
			int n = (int)recv(fd, RECV_CAST(req->body + req->body_len),
			                  (size_t)(clen - req->body_len), 0);
			if (n <= 0) { req->status = 400; break; }
			req->body_len += n;
		}
		req->body[req->body_len] = '\0';
	}
	return 1;
}

void req_free(s_http_req *req)
{
	free(req->body);
	req->body     = NULL;
	req->body_len = 0;
}

int buf_printf(char **dst, int *dstsz, int pos, const char *fmt, ...)
{
	va_list ap;
	int     needed;

	va_start(ap, fmt);
	needed = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);
	if (needed < 0) return pos;

	if (pos + needed + 1 >= *dstsz) {
		int newsz = *dstsz * 2;
		if (newsz < pos + needed + 8192) newsz = pos + needed + 8192;
		char *nb = (char *)realloc(*dst, (size_t)newsz);
		if (!nb) return pos;
		*dst   = nb;
		*dstsz = newsz;
	}
	va_start(ap, fmt);
	vsnprintf(*dst + pos, (size_t)(*dstsz - pos), fmt, ap);
	va_end(ap);
	return pos + needed;
}

int buf_json_string(char **dst, int *dstsz, int pos, const char *src)
{
    if (!src) src = "";
    size_t n = strlen(src);
    if (n > (SIZE_MAX - 8) / 6) return -1;
    size_t need = n * 6 + 1;
    if (need > (size_t)INT_MAX || pos < 0 || (size_t)pos > (size_t)INT_MAX - need) return -1;
    int required = pos + (int)need + 1;
    if (required >= *dstsz) {
        int newsz = *dstsz > 0 ? *dstsz * 2 : 8192;
        if (newsz < required) newsz = required;
        char *nb = (char *)realloc(*dst, (size_t)newsz);
        if (!nb) return -1;
        *dst = nb;
        *dstsz = newsz;
    }
    int wrote = json_escape(src, *dst + pos, *dstsz - pos);
    return pos + wrote;
}

void get_param(const char *qs, const char *key, char *out, int outsz)
{
	if (!out || outsz <= 0) return;
	out[0] = '\0';
	if (!qs || !*qs || !key || !*key) return;
	int         klen = (int)strlen(key);
	const char *p    = qs;
	while (*p) {
		if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
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
	if (!out || outsz <= 0) return;
	out[0] = '\0';
	if (!body || !key || !*key) return;
	int klen = (int)strlen(key);
	const char *p = body;
	while (*p) {
		if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
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

char *form_get_alloc(const char *body, const char *key)
{
	if (!body || !key || !*key) return NULL;
	size_t      klen = strlen(key);
	const char *p    = body;
	while (*p) {
		if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
			p += klen + 1;
			size_t n = 0;
			while (p[n] && p[n] != '&') n++;
			char *out = (char *)malloc(n + 1);
			if (!out) return NULL;
			memcpy(out, p, n);
			out[n] = '\0';
			url_decode(out);
			return out;
		}
		while (*p && *p != '&') p++;
		if (*p == '&') p++;
	}
	return NULL;
}

int write_file_atomic(const char *path, const char *data, size_t len)
{
	char tmp[CFGPATH_LEN + 8];
	int tn;
	if (!path || !data) return 0;
	tn = snprintf(tmp, sizeof(tmp), "%s.new", path);
	if (tn < 0 || (size_t)tn >= sizeof(tmp)) return 0;
	FILE *f = fopen(tmp, "wb");
	if (!f) return 0;
	int ok = (len == 0 || fwrite(data, 1, len, f) == len);
	if (fflush(f) != 0) ok = 0;
#ifndef _WIN32
	if (ok && fsync(fileno(f)) != 0) ok = 0;
#endif
	if (fclose(f) != 0) ok = 0;
	if (!ok) { remove(tmp); return 0; }
#ifdef TCMG_OS_WINDOWS
	if (!MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		remove(tmp);
		return 0;
	}
#else
	if (rename(tmp, path) != 0) { remove(tmp); return 0; }
#endif
	return 1;
}

int web_valid_text(const char *s)
{
	size_t n = strlen(s);
	if (!n) return 0;
	if (s[0] == ' ' || s[0] == '\t' || s[n - 1] == ' ' || s[n - 1] == '\t') return 0;
	for (; *s; s++) {
		unsigned char c = (unsigned char)*s;
		if (c < 0x20 || c == 0x7f || c == '#') return 0;
	}
	return 1;
}

int web_valid_ipv4_or_empty(const char *s)
{
	struct in_addr a;
	return !s[0] || inet_pton(AF_INET, s, &a) == 1;
}

int html_escape(const char *src, char *dst, int dstsz)
{
	int o = 0;
	for (const char *p = src; *p && o < dstsz - 7; p++) {
		switch (*p) {
		case '<':  memcpy(dst + o, "&lt;",   4); o += 4; break;
		case '>':  memcpy(dst + o, "&gt;",   4); o += 4; break;
		case '&':  memcpy(dst + o, "&amp;",  5); o += 5; break;
		case '"':  memcpy(dst + o, "&quot;", 6); o += 6; break;
		case '\'': memcpy(dst + o, "&#39;",  5); o += 5; break;
		default:   dst[o++] = *p; break;
		}
	}
	dst[o] = '\0';
	return o;
}

char *html_escape_alloc(const char *src, int maxbytes, int *truncated)
{
	int   srclen = (int)strlen(src);
	if (truncated) *truncated = (srclen > maxbytes);
	if (srclen > maxbytes) srclen = maxbytes;
	char *out = (char *)malloc(srclen * 6 + 8);
	if (!out) return NULL;
	html_escape(src, out, srclen * 6 + 8);
	return out;
}

char *file_read_escaped(const char *path, int maxbytes, int *truncated)
{
	if (truncated) *truncated = 0;
	FILE *fp = fopen(path, "r");
	if (!fp) {
		char *empty = (char *)malloc(1);
		if (empty) empty[0] = '\0';
		return empty;
	}
	char *raw = (char *)malloc(maxbytes + 1);
	if (!raw) { fclose(fp); return NULL; }
	int n = (int)fread(raw, 1, maxbytes, fp);
	raw[n] = '\0';
	if (truncated) *truncated = (n == maxbytes && fgetc(fp) != EOF);
	fclose(fp);

	char *out = html_escape_alloc(raw, n, NULL);
	free(raw);
	return out;
}

int json_escape(const char *src, char *dst, int dstsz)
{
	static const char hx[] = "0123456789abcdef";
	int o = 0;
	for (const unsigned char *p = (const unsigned char *)src; *p && o < dstsz - 7; p++) {
		unsigned char c = *p;
		if (c == '\r') continue;
		if      (c == '\n') { dst[o++] = '\\'; dst[o++] = 'n'; }
		else if (c == '\t') { dst[o++] = '\\'; dst[o++] = 't'; }
		else if (c == '"' || c == '\\') { dst[o++] = '\\'; dst[o++] = (char)c; }
		else if (c < 0x20) {
			dst[o++] = '\\'; dst[o++] = 'u'; dst[o++] = '0'; dst[o++] = '0';
			dst[o++] = hx[c >> 4]; dst[o++] = hx[c & 15];
		}
		else dst[o++] = (char)c;
	}
	dst[o] = '\0';
	return o;
}

void send_headers_ex(int fd, int code, const char *reason,
                     const char *ctype, int length, const char *set_cookie)
{
	char hdr[1280];
	time_t    now = time(NULL);
	struct tm tm_s;
	char      date_str[64];
	gmtime_r(&now, &tm_s);
	strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", &tm_s);

	char cookie_line[256] = "";
	if (set_cookie && set_cookie[0])
		snprintf(cookie_line, sizeof(cookie_line),
		         "Set-Cookie: tcmg_session=%s; Path=/; HttpOnly; SameSite=Strict\r\n",
		         set_cookie);

	int hdr_n = snprintf(hdr, sizeof(hdr),
	         "HTTP/1.1 %d %s\r\n"
	         "Server: %s\r\n"
	         "Date: %s\r\n"
	         "Content-Type: %s\r\n"
	         "Content-Length: %d\r\n"
	         "Cache-Control: no-store, no-cache\r\n"
	         "X-Content-Type-Options: nosniff\r\n"
	         "X-Frame-Options: DENY\r\n"
	         "Content-Security-Policy: frame-ancestors 'none'\r\n"
	         "Referrer-Policy: no-referrer\r\n"
	         "%s"
	         "Connection: close\r\n"
	         "\r\n",
	         code, reason, WEB_SERVER_NAME, date_str,
	         ctype, length, cookie_line);
	if (hdr_n >= (int)sizeof(hdr)) {
		tcmg_log("HTTP header truncated (needed %d bytes)", hdr_n);
		hdr_n = (int)sizeof(hdr) - 1;
	}
	send_all(fd, hdr, hdr_n);
}

void send_response_ex(int fd, int code, const char *reason,
                      const char *ctype, const char *body, int blen,
                      const char *set_cookie)
{
	send_headers_ex(fd, code, reason, ctype, blen, set_cookie);
	if (body && blen > 0)
		send_all(fd, body, blen);
}

void send_response(int fd, int code, const char *reason,
                   const char *ctype, const char *body, int blen)
{
	send_response_ex(fd, code, reason, ctype, body, blen, NULL);
}

void send_redirect(int fd, const char *location)
{
	char hdr[384];
	int n = snprintf(hdr, sizeof(hdr),
	         "HTTP/1.1 302 Found\r\nLocation: %s\r\nCache-Control: no-store\r\n"
	         "Content-Length: 0\r\nConnection: close\r\n\r\n", location);
	if (n >= (int)sizeof(hdr)) n = (int)sizeof(hdr) - 1;
	send_all(fd, hdr, n);
}

void send_redirect_with_cookie(int fd, const char *location, const char *token)
{
	char hdr[512];
	int n = snprintf(hdr, sizeof(hdr),
	         "HTTP/1.1 302 Found\r\nLocation: %s\r\n"
	         "Set-Cookie: tcmg_session=%s; Path=/; HttpOnly; SameSite=Strict; Max-Age=%d\r\n"
	         "Cache-Control: no-store\r\n"
	         "Content-Length: 0\r\nConnection: close\r\n\r\n",
	         location, token, WEB_SESSION_MAX_AGE);
	if (n >= (int)sizeof(hdr)) n = (int)sizeof(hdr) - 1;
	send_all(fd, hdr, n);
}

void send_redirect_clear_cookie(int fd, const char *location)
{
	char hdr[512];
	int n = snprintf(hdr, sizeof(hdr),
	         "HTTP/1.1 302 Found\r\nLocation: %s\r\n"
	         "Set-Cookie: tcmg_session=; Path=/; HttpOnly; SameSite=Strict;"
	         " Max-Age=0; Expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n"
	         "Cache-Control: no-store\r\n"
	         "Content-Length: 0\r\nConnection: close\r\n\r\n",
	         location);
	if (n >= (int)sizeof(hdr)) n = (int)sizeof(hdr) - 1;
	send_all(fd, hdr, n);
}

void send_webif_asset(int fd, const char *path)
{
	const char *body = NULL, *ctype = NULL;
	if (!strcmp(path, "/assets/app.css")) {
		body = TCMG_CSS;
		ctype = "text/css; charset=utf-8";
	} else if (!strcmp(path, "/assets/app.js")) {
		body = TCMG_JS;
		ctype = "application/javascript; charset=utf-8";
	} else if (!strcmp(path, "/assets/users.js")) {
		body = TCMG_USERS_JS;
		ctype = "application/javascript; charset=utf-8";
	} else if (!strcmp(path, "/assets/readers.js")) {
		body = TCMG_READERS_JS;
		ctype = "application/javascript; charset=utf-8";
	} else if (!strcmp(path, "/assets/livelog.js")) {
		body = TCMG_LIVELOG_JS;
		ctype = "application/javascript; charset=utf-8";
	} else {
		send_response(fd, 404, "Not Found", "text/plain", "not found", 9);
		return;
	}

	int len = (int)strlen(body);
	char hdr[512];
	int n = snprintf(hdr, sizeof(hdr),
	                 "HTTP/1.1 200 OK\r\n"
	                 "Server: %s\r\n"
	                 "Content-Type: %s\r\n"
	                 "Content-Length: %d\r\n"
	                 "Cache-Control: private, max-age=86400\r\n"
	                 "X-Content-Type-Options: nosniff\r\n"
	                 "Connection: close\r\n\r\n",
	                 WEB_SERVER_NAME, ctype, len);
	if (n < 0) return;
	if (n >= (int)sizeof(hdr)) n = (int)sizeof(hdr) - 1;
	send_all(fd, hdr, n);
	send_all(fd, body, len);
}

S_SERVER_STATS collect_stats(void)
{
	S_WEBIF_SERVER_STATS in = webif_server_stats();
	S_SERVER_STATS out;
	memset(&out, 0, sizeof(out));
	out.cw_found = in.cw_found;
	out.cw_not = in.cw_not;
	out.ecm_total = in.ecm_total;
	out.hit_rate = in.hit_rate;
	out.nbans = in.nbans;
	out.naccounts = in.naccounts;
	out.active_conns = in.active_conns;
	out.uptime_s = in.uptime_s;
	tcmg_strlcpy(out.uptime_str, in.uptime_str, sizeof(out.uptime_str));
	return out;
}

void handle_reset_stats(void)
{
	webif_account_reset_all_stats();
	tcmg_log("%s", "all user stats reset");
}

#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"
#pragma GCC diagnostic ignored "-Woverlength-strings"

#define ICO_LOGO \
 "<svg width='16' height='16' viewBox='0 0 24 24' fill='none'>" \
 "<path d='M12 2L2 7l10 5 10-5-10-5z' stroke='var(--p)' stroke-width='1.8' stroke-linejoin='round'/>" \
 "<path d='M2 17l10 5 10-5' stroke='var(--p)' stroke-width='1.8' stroke-linejoin='round'/>" \
 "<path d='M2 12l10 5 10-5' stroke='var(--cy)' stroke-width='1.8' stroke-linejoin='round'/>" \
 "</svg>"

#define ICO_THEME_BTN \
 "<button id='thBtn' class='thb' type='button' onclick='_theme_cycle()'" \
 " title='Theme' aria-label='Change theme'>" \
 "<svg class='ti-d i' viewBox='0 0 24 24'><path d='M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z'/></svg>" \
 "<svg class='ti-l i' viewBox='0 0 24 24'><circle cx='12' cy='12' r='4.5'/>" \
 "<path d='M12 2v2M12 20v2M4.93 4.93l1.41 1.41M17.66 17.66l1.41 1.41M2 12h2M20 12h2M4.93 19.07l1.41-1.41M17.66 6.34l1.41-1.41'/></svg>" \
 "</button>"

#define ICO_MENU \
 "<svg width='16' height='16' viewBox='0 0 24 24' fill='none'" \
 " stroke='currentColor' stroke-width='1.8'>" \
 "<line x1='3' y1='6' x2='21' y2='6'/>" \
 "<line x1='3' y1='12' x2='21' y2='12'/>" \
 "<line x1='3' y1='18' x2='21' y2='18'/></svg>"

int emit_header(char **buf, int *bsz, int pos,
                const char *title, const char *active)
{

    const char *nav_active = active;
    if (strcmp(active, "restart")  == 0 ||
        strcmp(active, "shutdown") == 0)
        nav_active = "power";
    int refresh = webif_max_refresh();

    pos = buf_printf(buf, bsz, pos,
        "<!DOCTYPE html><html lang='en' data-theme='dark' data-tpref='dark'><head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>TCMG &mdash; %s</title>"
        "<link rel='stylesheet' href='/assets/app.css?v=%s-20260926'>"
        "<script>" WEB_THEME_INIT_JS WEB_ACCENT_INIT_JS "window.TCMG_WEB_POLL=%d;</script>"
        "<script src='/assets/app.js?v=%s-20260926' defer></script>"
        "</head><body class='pg-%s'>"
        GLOBAL_ICON_SPRITE,
        title, TCMG_VERSION, refresh, TCMG_VERSION, active);

    pos = buf_printf(buf, bsz, pos,
        "<nav id='tb'>"
        "<div class='lo'>"
        "  <div class='li'>" ICO_LOGO "</div>"
        "  <span class='lt'>TCMG</span>"
        "  <span class='lv'>" TCMG_VERSION "</span>"
        "</div>"
        "<button id='mnuBtn' "
        "onclick='document.querySelector(\".tnav\").classList.toggle(\"open\")'>"
        ICO_MENU
        "</button>"
        "<div class='tnav'>");

    typedef struct { int sep; const char *id, *href, *icon, *label; } t_nav;

    static const char s_ico_status[] =
        "<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
        "<polyline points='22 12 18 12 15 21 9 3 6 12 2 12'/></svg>";
    static const char s_ico_log[] =
        "<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
        "<path d='M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z'/>"
        "<polyline points='14 2 14 8 20 8'/>"
        "<line x1='8' y1='13' x2='16' y2='13'/><line x1='8' y1='17' x2='16' y2='17'/></svg>";
    static const char s_ico_users[] =
        "<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
        "<path d='M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2'/>"
        "<circle cx='9' cy='7' r='4'/>"
        "<path d='M23 21v-2a4 4 0 0 0-3-3.87'/><path d='M16 3.13a4 4 0 0 1 0 7.75'/></svg>";
    static const char s_ico_ban[] =
        "<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
        "<circle cx='12' cy='12' r='10'/>"
        "<line x1='4.93' y1='4.93' x2='19.07' y2='19.07'/></svg>";
    static const char s_ico_cfg[] =
        "<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
        "<circle cx='12' cy='12' r='3'/>"
        "<path d='M19.07 4.93a10 10 0 0 1 0 14.14M4.93 4.93a10 10 0 0 0 0 14.14'/></svg>";
    static const char s_ico_files[] =
        "<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
        "<path d='M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z'/>"
        "<polyline points='14 2 14 8 20 8'/>"
        "<line x1='8' y1='13' x2='16' y2='13'/><line x1='8' y1='17' x2='12' y2='17'/></svg>";
    static const char s_ico_power[] =
        "<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
        "<path d='M18.36 6.64a9 9 0 1 1-12.73 0'/>"
        "<line x1='12' y1='2' x2='12' y2='12'/></svg>";
    static const char s_ico_tvcas[] =
        "<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
        "<path d='M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z'/></svg>";

    static const t_nav nav[] = {
        {0, "status",  "/status",  s_ico_status, "Dashboard"},
        {0, "livelog", "/livelog", s_ico_log,    "Live Log"},
        {1, NULL, NULL, NULL, NULL},
        {0, "users",   "/users",   s_ico_users,  "Users"},
        {0, "readers", "/readers", s_ico_cfg,    "Readers"},
        {0, "config",  "/config",  s_ico_cfg,    "Config"},
        {0, "failban", "/failban", s_ico_ban,    "Fail-Ban"},
        {1, NULL, NULL, NULL, NULL},
        {0, "files",   "/files",   s_ico_files,  "Files"},
        {0, "tvcas",   "/tvcas",   s_ico_tvcas,  "TVCAS"},
        {1, NULL, NULL, NULL, NULL},
        {0, "power",   "/power",   s_ico_power,  "Power"},
        {2, NULL, NULL, NULL, NULL},
    };

    for (int i = 0; nav[i].sep != 2; i++) {
        if (nav[i].sep == 1) {
            if (nav[i + 1].sep == 0)
                pos = buf_printf(buf, bsz, pos, "<div class='sep'></div>");
            continue;
        }
        const char *cls = (strcmp(nav[i].id, nav_active) == 0) ? " act" : "";
        pos = buf_printf(buf, bsz, pos,
            "<a href='%s' class='%s'>%s%s</a>",
            nav[i].href, cls, nav[i].icon, nav[i].label);
    }

    S_WEBIF_SERVER_STATS header_stats = webif_server_stats();

    pos = buf_printf(buf, bsz, pos,
        "</div>"
        "<div class='tbr'>"
        "  <div class='spill'>"
        "    <div class='pulse sm'></div>"
        "    <span id='tb_conn'>%d</span>&nbsp;online"
        "  </div>"
        "  <div class='pc%s'>"
        "    <label>AUTO</label>"
        "    <button onclick='_ap(-1)'>&#8722;</button>"
        "    <input id='ps_' type='text' value='%d' readonly>"
        "    <button onclick='_ap(1)'>+</button>"
        "  </div>"
        "  <div class='acp' id='acp'>"
        "    <button id='acBtn' class='thb' type='button' aria-haspopup='true' aria-expanded='false'"
        "     title='Accent color' aria-label='Accent color'>"
        "      <svg class='i' viewBox='0 0 24 24'>"
        "        <circle cx='13.5' cy='6.5' r='.5' fill='currentColor'/>"
        "        <circle cx='17.5' cy='10.5' r='.5' fill='currentColor'/>"
        "        <circle cx='8.5' cy='7.5' r='.5' fill='currentColor'/>"
        "        <circle cx='6.5' cy='12.5' r='.5' fill='currentColor'/>"
        "        <path d='M12 2C6.5 2 2 6.5 2 12s4.5 10 10 10c1.1 0 2-.9 2-2 0-.5-.2-1-.5-1.4-.3-.4-.5-.9-.5-1.4"
        "0-1.1.9-2 2-2h2.4c2.3 0 4.1-1.8 4.1-4.1C21.5 6 17.2 2 12 2z'/>"
        "      </svg>"
        "    </button>"
        "    <div class='acpop' id='acPop' hidden role='menu' aria-label='Accent color'></div>"
        "  </div>"
        ICO_THEME_BTN
        "</div>"
        "</nav>"
        "<div id='mn'><div id='ct'>",
        header_stats.active_conns,
        refresh <= 0 ? " pc-off" : "",
        refresh > 0 ? refresh : 5);

    return pos;
}

static unsigned long webif_memory_kb(void)
{
#ifndef TCMG_OS_WINDOWS
#if defined(__linux__)
    FILE *fp = fopen("/proc/self/status", "r");
    if (fp) {
        char line[128];
        while (fgets(line, sizeof(line), fp)) {
            unsigned long kb = 0;
            if (sscanf(line, "VmRSS: %lu kB", &kb) == 1) {
                fclose(fp);
                return kb;
            }
        }
        fclose(fp);
    }
#endif
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) == 0) {
#if defined(__APPLE__)
        return (unsigned long)(ru.ru_maxrss / 1024UL);
#else
        return (unsigned long)ru.ru_maxrss;
#endif
    }
#endif
    return 0;
}

int emit_footer(char **buf, int *bsz, int pos)
{
    unsigned long mem_kb = webif_memory_kb();
    char mem[64];
    if (mem_kb > 0) {
        if (mem_kb >= 1024UL)
            snprintf(mem, sizeof(mem), "Memory usage %.1f MiB", (double)mem_kb / 1024.0);
        else
            snprintf(mem, sizeof(mem), "Memory usage %lu KiB", mem_kb);
    } else {
        tcmg_strlcpy(mem, "Memory usage n/a", sizeof(mem));
    }

    return buf_printf(buf, bsz, pos,
        "</div>"
        "</div>"
        "<footer class='site-footer'>"
        "<span class='site-footer-brand'>TCMG <span class='site-footer-version'>" TCMG_VERSION "</span></span>"
        "<span class='site-footer-sep'>&bull;</span>"
        "<span>built " TCMG_BUILD_TIME "</span>"
        "<span class='site-footer-sep'>&bull;</span>"
        "<span id='mem_usage'>%s</span>"
        "</footer>"
        "</body></html>", mem);
}
