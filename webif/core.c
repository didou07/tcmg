#define MODULE_LOG_PREFIX "webif"
#include "../globals.h"
#include "internal/proto.h"
#include "assets/webif_assets.h"


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
	if (!g_cfg.webif_user[0] && !g_cfg.webif_pass[0]) return 1;
	if (!auth_header) return 0;
	const char *p = strstr(auth_header, "Basic ");
	if (!p) return 0;
	p += 6;
	char got[512]; int glen = 0;
	while (*p && *p != '\r' && *p != '\n' && *p != ' ' && glen < (int)sizeof(got) - 1)
		got[glen++] = *p++;
	got[glen] = '\0';
	char creds[256];
	snprintf(creds, sizeof(creds), "%s:%s", g_cfg.webif_user, g_cfg.webif_pass);
	char expected[512];
	b64_encode(creds, (int)strlen(creds), expected, sizeof(expected));
	return ct_streq(got, expected) ? 1 : 0;
}

int check_credentials(const char *user, const char *pass)
{
	if (!g_cfg.webif_user[0] && !g_cfg.webif_pass[0]) return 1;
	if (!user || !pass) return 0;
	return (ct_streq(user, g_cfg.webif_user) &&
	        ct_streq(pass, g_cfg.webif_pass)) ? 1 : 0;
}


void url_decode(char *s)
{
	char *r = s, *w = s;
	while (*r) {
		if (*r == '%' && r[1] && r[2]) {
			char h[3] = { r[1], r[2], 0 };
			*w++ = (char)strtol(h, NULL, 16);
			r += 3;
		} else if (*r == '+') { *w++ = ' '; r++; }
		else                  { *w++ = *r++; }
	}
	*w = '\0';
}

static void read_post_body(int fd, s_http_req *req, const char *raw, int rawlen)
{
	(void)rawlen;
	const char *cl = strstr(raw, "\r\nContent-Length:");
	if (!cl) cl = strstr(raw, "\nContent-Length:");
	if (!cl) return;
	cl = strchr(cl, ':');
	if (!cl) return;
	cl++;
	while (*cl == ' ') cl++;
	int clen = atoi(cl);
	if (clen <= 0 || clen > WEB_POST_MAX) return;

	if (clen > req->body_len) {
		char *nb = (char *)realloc(req->body, clen + 1);
		if (!nb) return;
		req->body = nb;
	}
	while (req->body_len < clen) {
		int n = (int)recv(fd, RECV_CAST(req->body + req->body_len),
		                  clen - req->body_len, 0);
		if (n <= 0) break;
		req->body_len += n;
	}
	req->body[req->body_len] = '\0';
}

int req_parse(s_http_req *req, int fd, char *raw, int rawlen)
{
	memset(req, 0, sizeof(*req));
	char uri[512] = {0};
	if (sscanf(raw, "%7s %511s", req->method, uri) < 2)
		return 0;

	tcmg_strlcpy(req->path, uri, sizeof(req->path));
	char *q = strchr(req->path, '?');
	if (q) { *q = '\0'; tcmg_strlcpy(req->qs, q + 1, sizeof(req->qs)); }

	if (strcmp(req->method, "POST") == 0) {
		const char *bs = strstr(raw, "\r\n\r\n");
		if (bs) {
			bs += 4;
			req->body_len = (int)(raw + rawlen - bs);
			if (req->body_len < 0) req->body_len = 0;
			if (req->body_len > WEB_POST_MAX) req->body_len = WEB_POST_MAX;
			req->body = (char *)malloc(req->body_len + 1);
			if (req->body) {
				memcpy(req->body, bs, req->body_len);
				req->body[req->body_len] = '\0';
				read_post_body(fd, req, raw, rawlen);
			}
		}
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

	if (pos + needed + 1 >= *dstsz) {
		int   newsz = pos + needed + 8192;
		char *nb    = (char *)realloc(*dst, newsz);
		if (!nb) return pos;
		*dst   = nb;
		*dstsz = newsz;
	}
	va_start(ap, fmt);
	vsnprintf(*dst + pos, *dstsz - pos, fmt, ap);
	va_end(ap);
	return pos + needed;
}

void get_param(const char *qs, const char *key, char *out, int outsz)
{
	out[0] = '\0';
	if (!qs || !*qs) return;
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
	out[0] = '\0';
	if (!body) return;
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
	if (n < 0) n = 0;
	raw[n] = '\0';
	if (truncated) *truncated = !feof(fp);
	fclose(fp);

	char *out = html_escape_alloc(raw, n, NULL);
	free(raw);
	return out;
}

int json_escape(const char *src, char *dst, int dstsz)
{
	int o = 0;
	for (const char *p = src; *p && o < dstsz - 6; p++) {
		if (*p == '\r') continue;
		if (*p == '\n') { dst[o++] = '\\'; dst[o++] = 'n'; continue; }
		if (*p == '"'  || *p == '\\') { dst[o++] = '\\'; }
		dst[o++] = *p;
	}
	dst[o] = '\0';
	return o;
}

void send_headers_ex(int fd, int code, const char *reason,
                     const char *ctype, int length, const char *set_cookie)
{
	char hdr[768];
	time_t    now = time(NULL);
	struct tm tm_s;
	char      date_str[64];
	gmtime_r(&now, &tm_s);
	strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", &tm_s);

	char cookie_line[256] = "";
	if (set_cookie && set_cookie[0])
		snprintf(cookie_line, sizeof(cookie_line),
		         "Set-Cookie: tcmg_session=%s; Path=/; HttpOnly; SameSite=Lax\r\n",
		         set_cookie);

	int hdr_n = snprintf(hdr, sizeof(hdr),
	         "HTTP/1.1 %d %s\r\n"
	         "Server: %s\r\n"
	         "Date: %s\r\n"
	         "Content-Type: %s\r\n"
	         "Content-Length: %d\r\n"
	         "Cache-Control: no-store, no-cache\r\n"
	         "%s"
	         "Connection: close\r\n"
	         "\r\n",
	         code, reason, WEB_SERVER_NAME, date_str,
	         ctype, length, cookie_line);
	if (hdr_n >= (int)sizeof(hdr))
		tcmg_log("webif: HTTP header truncated (needed %d bytes)", hdr_n);
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
	         "Set-Cookie: tcmg_session=%s; Path=/; HttpOnly; SameSite=Lax; Max-Age=%d\r\n"
	         "Cache-Control: no-store\r\n"
	         "Content-Length: 0\r\nConnection: close\r\n\r\n",
	         location, token, WEB_SESSION_TIMEOUT);
	send(fd, SO_CAST(hdr), (int)strlen(hdr), MSG_NOSIGNAL);
}

void send_redirect_clear_cookie(int fd, const char *location)
{
	char hdr[512];
	snprintf(hdr, sizeof(hdr),
	         "HTTP/1.1 302 Found\r\nLocation: %s\r\n"
	         "Set-Cookie: tcmg_session=; Path=/; HttpOnly; SameSite=Lax;"
	         " Max-Age=0; Expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n"
	         "Cache-Control: no-store\r\n"
	         "Content-Length: 0\r\nConnection: close\r\n\r\n",
	         location);
	send(fd, SO_CAST(hdr), (int)strlen(hdr), MSG_NOSIGNAL);
}


S_SERVER_STATS collect_stats(void)
{
	S_SERVER_STATS s;
	memset(&s, 0, sizeof(s));
	time_t now = time(NULL);

	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	s.naccounts = g_cfg.naccounts;
	for (const S_ACCOUNT *a = g_cfg.accounts; a; a = a->next) {
		s.cw_found += a->cw_found;
		s.cw_not   += a->cw_not;
	}
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	pthread_mutex_lock(&g_cfg.ban_lock);
	for (int _bi = 0; _bi < BAN_BUCKETS; _bi++)
		for (const S_BAN_ENTRY *b = g_cfg.ban_table[_bi]; b; b = b->next)
			if (now < b->until) s.nbans++;
	pthread_mutex_unlock(&g_cfg.ban_lock);

	s.ecm_total    = s.cw_found + s.cw_not;
	s.hit_rate     = s.ecm_total > 0
	               ? (double)s.cw_found * 100.0 / (double)s.ecm_total
	               : 0.0;
	s.active_conns = g_active_conns;
	s.uptime_s     = now - g_start_time;
	format_uptime(s.uptime_s, s.uptime_str, sizeof(s.uptime_str));
	return s;
}

void handle_reset_stats(void)
{
	pthread_rwlock_wrlock(&g_cfg.acc_lock);
	for (S_ACCOUNT *a = g_cfg.accounts; a; a = a->next) {
		pthread_mutex_lock(&a->stat_mtx);
		a->ecm_total        = 0;
		a->cw_found         = 0;
		a->cw_not           = 0;
		a->cw_time_total_ms = 0;
		a->cw_time_min_ms   = 0;
		a->cw_time_max_ms   = 0;
		a->first_login      = 0;
		a->last_seen        = 0;
		pthread_mutex_unlock(&a->stat_mtx);
	}
	pthread_rwlock_unlock(&g_cfg.acc_lock);
	tcmg_log("%s", "webif: all user stats reset");
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

    pos = buf_printf(buf, bsz, pos,
        "<!DOCTYPE html><html lang='en'><head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>TCMG &mdash; %s</title>"
        "<style>%s</style>"
        "</head><body class='pg-%s'>",
        title, TCMG_CSS, active);

    char srv_addr[64];
    snprintf(srv_addr, sizeof(srv_addr), "%s:%d",
             g_cfg.webif_bindaddr[0] ? g_cfg.webif_bindaddr : "0.0.0.0",
             g_cfg.webif_port);

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

    int refresh = g_cfg.webif_refresh;

    pos = buf_printf(buf, bsz, pos,
        "</div>"
        "<div class='tbr'>"
        "  <div class='spill'>"
        "    <div class='pulse sm'></div>"
        "    <span id='tb_conn'>%d</span>&nbsp;online"
        "  </div>"
        "  <span class='chip' id='tb_addr'>%s</span>"
        "  <div class='pc%s'>"
        "    <label>AUTO</label>"
        "    <button onclick='_ap(-1)'>&#8722;</button>"
        "    <input id='ps_' type='text' value='%d' readonly>"
        "    <button onclick='_ap(1)'>+</button>"
        "  </div>"
        "</div>"
        "</nav>"
        "<div id='mn'><div id='ct'>",
        g_active_conns, srv_addr,
        refresh <= 0 ? " pc-off" : "",
        refresh > 0 ? refresh : 5);

    pos = buf_printf(buf, bsz, pos,
        "<script>" TCMG_JS "</script>",
        refresh);

    return pos;
}

int emit_footer(char **buf, int *bsz, int pos)
{
    return buf_printf(buf, bsz, pos,
        "</div>"
        "</div>"
        "<footer style='"
        "padding:10px 22px;border-top:1px solid var(--bd);"
        "display:flex;align-items:center;justify-content:center;"
        "font-size:11px;color:var(--t2);font-family:var(--mono)'>"
        "TCMG <span style='color:var(--p);margin:0 4px'>" TCMG_VERSION "</span>"
        "&bull; built <span style='color:var(--t1);margin-left:4px'>"
        TCMG_BUILD_TIME
        "</span></footer>"
        "</body></html>");
}
