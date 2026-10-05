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
	static const char hex[] = "0123456789abcdef";
	uint8_t rnd[16];
	csprng(rnd, sizeof(rnd));
	for (int i = 0; i < 16; i++) {
		out[i * 2] = hex[rnd[i] >> 4];
		out[i * 2 + 1] = hex[rnd[i] & 0x0F];
	}
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
	static const char key[] = "tcmg_session=";
	if (!cookie_hdr || !buf || bufsz <= 0) return NULL;
	const char *p = cookie_hdr;
	while (*p) {
		while (*p == ' ' || *p == '\t' || *p == ';') p++;
		if (!strncmp(p, key, sizeof(key) - 1)) {
			p += sizeof(key) - 1;
			int i = 0;
			while (i < bufsz - 1 && p[i] && p[i] != ';' && p[i] != '\r' && p[i] != '\n' && p[i] != ' ' && p[i] != '\t') {
				buf[i] = p[i];
				i++;
			}
			buf[i] = '\0';
			if (i == WEB_SESSION_LEN) return buf;
			return NULL;
		}
		while (*p && *p != ';' && *p != '\r' && *p != '\n') p++;
	}
	return NULL;
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
	if (!out || outsz <= 0) return;
	if (!in || ilen <= 0) { out[0] = '\0'; return; }
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
	if (!s) return;
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
	if (!raw || !name || !buf || bufsz <= 0) return NULL;
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
		int k = (int)send(fd, SO_CAST(p), (size_t)n, MSG_NOSIGNAL);
		if (k < 0) { if (errno == EINTR) continue; return -1; }
		if (k == 0) return -1;
		p += k; n -= k;
	}
	return 0;
}

static int send_all2(int fd, const char *a, int an, const char *b, int bn)
{
#ifndef TCMG_OS_WINDOWS
	while (an > 0 || bn > 0) {
		struct iovec iov[2];
		int c = 0;
		if (an > 0) { iov[c].iov_base = (void *)a; iov[c].iov_len = (size_t)an; c++; }
		if (bn > 0) { iov[c].iov_base = (void *)b; iov[c].iov_len = (size_t)bn; c++; }
		struct msghdr mh;
		memset(&mh, 0, sizeof(mh));
		mh.msg_iov    = iov;
		mh.msg_iovlen = (size_t)c;
		ssize_t k = sendmsg(fd, &mh, MSG_NOSIGNAL);
		if (k < 0) { if (errno == EINTR) continue; return -1; }
		if (k == 0) return -1;
		if (an > 0) {
			int t = k < an ? (int)k : an;
			a += t; an -= t; k -= t;
		}
		if (k > 0) { b += k; bn -= (int)k; }
	}
	return 0;
#else
	if (an > 0 && send_all(fd, a, an) != 0) return -1;
	return bn > 0 ? send_all(fd, b, bn) : 0;
#endif
}

int req_parse(s_http_req *req, int fd, char *raw, int rawlen)
{
	if (!req || !raw || rawlen <= 0) return 0;
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
	if (!req) return;
	free(req->body);
	req->body     = NULL;
	req->body_len = 0;
}

int buf_printf(char **dst, int *dstsz, int pos, const char *fmt, ...)
{
	if (!dst || !*dst || !dstsz || !fmt || *dstsz <= 0 || pos < 0 || pos >= *dstsz) return pos;

	int avail = *dstsz - pos;
	if (!strchr(fmt, '%')) {
		int n = (int)strlen(fmt);
		if (n < avail) {
			memcpy(*dst + pos, fmt, (size_t)n + 1);
			return pos + n;
		}
		int newsz = *dstsz * 2;
		if (newsz < pos + n + 2048) newsz = pos + n + 2048;
		char *nb = (char *)realloc(*dst, (size_t)newsz);
		if (!nb) return pos;
		*dst = nb;
		*dstsz = newsz;
		memcpy(*dst + pos, fmt, (size_t)n + 1);
		return pos + n;
	}

	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(*dst + pos, (size_t)avail, fmt, ap);
	va_end(ap);
	if (n < 0) return pos;
	if (n < avail) return pos + n;

	int newsz = *dstsz * 2;
	if (newsz < pos + n + 2048) newsz = pos + n + 2048;
	char *nb = (char *)realloc(*dst, (size_t)newsz);
	if (!nb) return pos;
	*dst = nb;
	*dstsz = newsz;
	va_start(ap, fmt);
	vsnprintf(*dst + pos, (size_t)(*dstsz - pos), fmt, ap);
	va_end(ap);
	return pos + n;
}

int buf_json_string(char **dst, int *dstsz, int pos, const char *src)
{
    if (!dst || !dstsz || pos < 0) return -1;
    if (pos > 0 && (!*dst || *dstsz <= pos)) return -1;
    if (!src) src = "";
    size_t n = strlen(src);
    if (n > (SIZE_MAX - 8) / 6) return -1;
    size_t need = n * 6 + 1;
    if (need > (size_t)INT_MAX || (size_t)pos > (size_t)INT_MAX - need) return -1;
    int required = pos + (int)need + 1;
    if (!*dst || *dstsz <= 0 || required >= *dstsz) {
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

static const char *web_param_value(const char *query, const char *key, size_t *value_len)
{
    if (value_len) *value_len = 0;
    if (!query || !key || !*key) return NULL;
    const size_t key_len = strlen(key);
    const char *p = query;
    while (*p) {
        if (strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            const char *value = p + key_len + 1;
            size_t len = 0;
            while (value[len] && value[len] != '&') len++;
            if (value_len) *value_len = len;
            return value;
        }
        while (*p && *p != '&') p++;
        if (*p == '&') p++;
    }
    return NULL;
}

void get_param(const char *qs, const char *key, char *out, int outsz)
{
    if (!out || outsz <= 0) return;
    out[0] = '\0';
    size_t len = 0;
    const char *value = web_param_value(qs, key, &len);
    if (!value) return;
    if (len >= (size_t)outsz) len = (size_t)outsz - 1;
    memcpy(out, value, len);
    out[len] = '\0';
    url_decode(out);
}

int form_get_copy(const char *body, const char *key, char *out, size_t outsz)
{
    if (!out || outsz == 0) return -1;
    out[0] = '\0';
    if (!body || !key || !*key) return 0;
    size_t len = 0;
    const char *value = web_param_value(body, key, &len);
    if (!value) return 0;
    size_t w = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)value[i];
        if (c == '%' && i + 2 < len) {
            int hi = hexval(value[i + 1]), lo = hexval(value[i + 2]);
            if (hi >= 0 && lo >= 0) {
                c = (unsigned char)((hi << 4) | lo);
                i += 2;
                if (c == 0) continue;
            }
        } else if (c == '+') c = ' ';
        if (w + 1 >= outsz) { out[0] = '\0'; return -1; }
        out[w++] = (char)c;
    }
    out[w] = '\0';
    return 1;
}

int form_has(const char *body, const char *key)
{
    return web_param_value(body, key, NULL) != NULL;
}

void form_get(const char *body, const char *key, char *out, int outsz)
{
    if (!out || outsz <= 0) return;
    get_param(body, key, out, outsz);
}

char *form_get_alloc(const char *body, const char *key)
{
    size_t len = 0;
    const char *value = web_param_value(body, key, &len);
    if (!value) return NULL;
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, value, len);
    out[len] = '\0';
    url_decode(out);
    return out;
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
	if (!s) return 0;
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
	if (!s) return 0;
	struct in_addr a;
	return !s[0] || inet_pton(AF_INET, s, &a) == 1;
}

static size_t html_escape_write(const char *src, size_t len, char *dst, size_t cap)
{
	if (!dst || cap == 0) return 0;
	if (!src) len = 0;
	size_t o = 0;
	for (size_t i = 0; i < len; i++) {
		const char *rep = NULL;
		size_t n = 1;
		switch ((unsigned char)src[i]) {
		case '<':  rep = "&lt;";   n = 4; break;
		case '>':  rep = "&gt;";   n = 4; break;
		case '&':  rep = "&amp;";  n = 5; break;
		case '"':  rep = "&quot;"; n = 6; break;
		case '\'': rep = "&#39;";  n = 5; break;
		default: break;
		}
		if (o + n + 1 > cap) break;
		if (rep) memcpy(dst + o, rep, n);
		else dst[o] = src[i];
		o += n;
	}
	dst[o] = '\0';
	return o;
}

int html_escape(const char *src, char *dst, int dstsz)
{
	if (!dst || dstsz <= 0) return 0;
	if (!src) src = "";
	return (int)html_escape_write(src, strlen(src), dst, (size_t)dstsz);
}

static size_t html_escaped_len(const char *buf, size_t len)
{
	size_t n = 0;
	for (size_t i = 0; i < len; i++) {
		switch ((unsigned char)buf[i]) {
		case '<': case '>': n += 4; break;
		case '&': n += 5; break;
		case '"': n += 6; break;
		case '\'': n += 5; break;
		default: n += 1; break;
		}
	}
	return n;
}

static size_t html_escape_inplace(char *buf, size_t len, size_t cap)
{
	size_t src = len;
	size_t dst = cap;
	while (src > 0) {
		unsigned char c = (unsigned char)buf[--src];
		const char *rep = NULL;
		switch (c) {
		case '<': rep = "&lt;"; break;
		case '>': rep = "&gt;"; break;
		case '&': rep = "&amp;"; break;
		case '"': rep = "&quot;"; break;
		case '\'': rep = "&#39;"; break;
		default: buf[--dst] = (char)c; continue;
		}
		size_t n = strlen(rep);
		while (n) buf[--dst] = rep[--n];
	}
	size_t outlen = cap - dst;
	memmove(buf, buf + dst, outlen);
	buf[outlen] = '\0';
	return outlen;
}

int buf_html_string(char **dst, int *dstsz, int pos, const char *src)
{
	if (!dst || !*dst || !dstsz || pos < 0) return -1;
	if (!src) src = "";
	size_t n = strlen(src);
	size_t need = html_escaped_len(src, n);
	if (need > (size_t)(INT_MAX / 2) || (size_t)pos > (size_t)(INT_MAX / 2)) return -1;
	int required = pos + (int)need + 1;
	if (required > *dstsz) {
		int newsz = *dstsz * 2;
		if (newsz < required + 1024) newsz = required + 1024;
		char *nb = (char *)realloc(*dst, (size_t)newsz);
		if (!nb) return -1;
		*dst = nb;
		*dstsz = newsz;
	}
	if (html_escape_write(src, n, *dst + pos, (size_t)(*dstsz - pos)) != need) return -1;
	return pos + (int)need;
}

char *html_escape_alloc(const char *src, int maxbytes, int *truncated)
{
	if (!src) src = "";
	if (maxbytes < 0) maxbytes = 0;
	size_t srclen = strlen(src);
	if (truncated) *truncated = srclen > (size_t)maxbytes;
	if (srclen > (size_t)maxbytes) srclen = (size_t)maxbytes;
	size_t cap = html_escaped_len(src, srclen) + 1;
	char *out = (char *)malloc(cap);
	if (!out) return NULL;
	if (srclen) memcpy(out, src, srclen);
	html_escape_inplace(out, srclen, cap);
	return out;
}

char *file_read_escaped(const char *path, int maxbytes, int *truncated)
{
	if (truncated) *truncated = 0;
	if (maxbytes < 0) maxbytes = 0;
	FILE *fp = fopen(path, "r");
	if (!fp) {
		char *empty = (char *)malloc(1);
		if (empty) empty[0] = '\0';
		return empty;
	}
	struct stat st;
	if (fstat(fileno(fp), &st) != 0 || st.st_size < 0) {
		fclose(fp);
		char *empty = (char *)malloc(1);
		if (empty) empty[0] = '\0';
		return empty;
	}
	size_t limit = (size_t)st.st_size;
	if (limit > (size_t)maxbytes) {
		limit = (size_t)maxbytes;
		if (truncated) *truncated = 1;
	}
	char *buf = (char *)malloc(limit + 1);
	if (!buf) { fclose(fp); return NULL; }
	size_t n = fread(buf, 1, limit, fp);
	if (ferror(fp)) { fclose(fp); free(buf); return NULL; }
	fclose(fp);
	size_t cap = html_escaped_len(buf, n) + 1;
	if (cap < limit + 1) cap = limit + 1;
	if (cap > limit + 1) {
		char *nb = (char *)realloc(buf, cap);
		if (!nb) { free(buf); return NULL; }
		buf = nb;
	}
	if (n < limit && st.st_size > (off_t)n) {
		if ((size_t)n < (size_t)maxbytes) {
			if (truncated) *truncated = 0;
		}
	}
	html_escape_inplace(buf, n, cap);
	return buf;
}

int json_escape(const char *src, char *dst, int dstsz)
{
	if (!dst || dstsz <= 0) return 0;
	if (!src) src = "";
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

static int build_headers(char *hdr, size_t cap, int code, const char *reason,
                         const char *ctype, int length, const char *set_cookie)
{
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

	int hdr_n = snprintf(hdr, cap,
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
	if (hdr_n < 0) return 0;
	if (hdr_n >= (int)cap) {
		tcmg_log("HTTP header truncated (needed %d bytes)", hdr_n);
		hdr_n = (int)cap - 1;
	}
	return hdr_n;
}

void send_headers_ex(int fd, int code, const char *reason,
                     const char *ctype, int length, const char *set_cookie)
{
	char hdr[1280];
	int  n = build_headers(hdr, sizeof(hdr), code, reason, ctype, length, set_cookie);
	if (n > 0) send_all(fd, hdr, n);
}

void send_response_ex(int fd, int code, const char *reason,
                      const char *ctype, const char *body, int blen,
                      const char *set_cookie)
{
	char hdr[1280];
	int  n = build_headers(hdr, sizeof(hdr), code, reason, ctype, blen, set_cookie);
	if (n <= 0) return;
	send_all2(fd, hdr, n, body, blen > 0 ? blen : 0);
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
	int len = 0;
	if (!strcmp(path, "/assets/app.css")) {
		body = TCMG_CSS;
		len = (int)TCMG_CSS_LEN;
		ctype = "text/css; charset=utf-8";
	} else if (!strcmp(path, "/assets/app.js")) {
		body = TCMG_JS;
		len = (int)TCMG_JS_LEN;
		ctype = "application/javascript; charset=utf-8";
	} else if (!strcmp(path, "/assets/users.js")) {
		body = TCMG_USERS_JS;
		len = (int)TCMG_USERS_JS_LEN;
		ctype = "application/javascript; charset=utf-8";
	} else if (!strcmp(path, "/assets/readers.js")) {
		body = TCMG_READERS_JS;
		len = (int)TCMG_READERS_JS_LEN;
		ctype = "application/javascript; charset=utf-8";
	} else if (!strcmp(path, "/assets/livelog.js")) {
		body = TCMG_LIVELOG_JS;
		len = (int)TCMG_LIVELOG_JS_LEN;
		ctype = "application/javascript; charset=utf-8";
	} else {
		send_response(fd, 404, "Not Found", "text/plain", "not found", 9);
		return;
	}

	char hdr[512];
	int n = snprintf(hdr, sizeof(hdr),
	                 "HTTP/1.1 200 OK\r\n"
	                 "Server: %s\r\n"
	                 "Content-Type: %s\r\n"
	                 "Content-Length: %d\r\n"
	                 "Cache-Control: private, max-age=31536000, immutable\r\n"
	                 "X-Content-Type-Options: nosniff\r\n"
	                 "Connection: close\r\n\r\n",
	                 WEB_SERVER_NAME, ctype, len);
	if (n < 0) return;
	if (n >= (int)sizeof(hdr)) n = (int)sizeof(hdr) - 1;
	send_all2(fd, hdr, n, body, len);
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


#define ICO_LOGO \
 "<svg width='18' height='18' viewBox='0 0 24 24' fill='none' aria-hidden='true'>" \
 "<rect x='3' y='4' width='18' height='16' rx='3' stroke='var(--p)' stroke-width='1.8'/>" \
 "<rect x='7' y='8' width='5' height='5' rx='1' stroke='var(--cy)' stroke-width='1.5'/>" \
 "<path d='M14.5 9.5h3M14.5 12h3M7 16h10' stroke='var(--p)' stroke-width='1.5' stroke-linecap='round'/>" \
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
    int refresh = webif_max_refresh();

    pos = buf_printf(buf, bsz, pos,
        "<!DOCTYPE html><html lang='en'><head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>TCMG &mdash; %s</title>"
        "<link rel='stylesheet' href='/assets/app.css?v=%s'>"
        "<script>window.TCMG_WEB_POLL=%d;</script>"
        "<script src='/assets/app.js?v=%s' defer></script>"
        "</head><body class='pg-%s'>%s",
        title, TCMG_ASSET_REV, refresh, TCMG_ASSET_REV, active, GLOBAL_ICON_SPRITE);

    pos = buf_printf(buf, bsz, pos,
        "<nav id='tb'>"
        "<div class='lo'>"
        "  <div class='li'>" ICO_LOGO "</div>"
        "  <span class='lt'>TCMG</span>"
        "  <span class='lv'>" TCMG_VERSION "</span>"
        "</div>"
        "<span class='mpt' aria-live='polite'>%s</span>"
        "<button id='mnuBtn' type='button' aria-label='Open menu' aria-controls='mobile-nav' aria-expanded='false' "
        "onclick='toggleMobileNav(this)'>"
        ICO_MENU
        "</button>"
        "<div class='tnav' id='mobile-nav'>",
        title);

    typedef struct { int sep; const char *id, *href, *icon, *label; } t_nav;

    static const char s_ico_status[] = "<svg class='ni' viewBox='0 0 24 24'><use href='#i-activity'/></svg>";
    static const char s_ico_log[]    = "<svg class='ni' viewBox='0 0 24 24'><use href='#i-terminal'/></svg>";
    static const char s_ico_users[]  = "<svg class='ni' viewBox='0 0 24 24'><use href='#i-users'/></svg>";
    static const char s_ico_ban[]    = "<svg class='ni' viewBox='0 0 24 24'><use href='#i-ban'/></svg>";
    static const char s_ico_reader[] = "<svg class='ni' viewBox='0 0 24 24'><use href='#i-server'/></svg>";
    static const char s_ico_cfg[]    = "<svg class='ni' viewBox='0 0 24 24'><use href='#i-sliders'/></svg>";
    static const char s_ico_files[]  = "<svg class='ni' viewBox='0 0 24 24'><use href='#i-file-text'/></svg>";
    static const char s_ico_power[]  = "<svg class='ni' viewBox='0 0 24 24'><use href='#i-power'/></svg>";
    static const char s_ico_tvcas[]  = "<svg class='ni' viewBox='0 0 24 24'><use href='#i-shield'/></svg>";

    static const t_nav nav[] = {
        {0, "status",  "/status",  s_ico_status, "Dashboard"},
        {0, "livelog", "/livelog", s_ico_log,    "Live Log"},
        {1, NULL, NULL, NULL, NULL},
        {0, "users",   "/users",   s_ico_users,  "Users"},
        {0, "readers", "/readers", s_ico_reader, "Readers"},
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
            "<a href='%s' class='%s' onclick='closeMobileNav()'>%s<span class='nav-label'>%s</span></a>",
            nav[i].href, cls, nav[i].icon, nav[i].label);
    }

    int header_conns = webif_active_connection_count();

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
        "</div>"
        "</div>"
        "<script>function toggleMobileNav(b){var n=document.getElementById('mobile-nav');if(!n)return;var o=!n.classList.contains('open');n.classList.toggle('open',o);document.body.classList.toggle('nav-open',o);if(b){b.setAttribute('aria-expanded',o?'true':'false');b.setAttribute('aria-label',o?'Close menu':'Open menu');}}function closeMobileNav(){var n=document.getElementById('mobile-nav'),b=document.getElementById('mnuBtn');if(n)n.classList.remove('open');document.body.classList.remove('nav-open');if(b){b.setAttribute('aria-expanded','false');b.setAttribute('aria-label','Open menu');}}document.addEventListener('keydown',function(e){if(e.key==='Escape')closeMobileNav();});</script>"
        "</nav>"
        "<div id='mn'><div id='ct'>",
        header_conns,
        refresh <= 0 ? " pc-off" : "",
        refresh > 0 ? refresh : 5);

    return pos;
}

static unsigned long webif_memory_kb(void)
{
    static _Atomic unsigned long cached_kb = 0;
    static _Atomic time_t cached_at = 0;
    time_t now = time(NULL);
    time_t at = atomic_load_explicit(&cached_at, memory_order_relaxed);
    unsigned long cached = atomic_load_explicit(&cached_kb, memory_order_relaxed);
    if (cached && now - at < 2) return cached;
#ifndef TCMG_OS_WINDOWS
#if defined(__linux__)
    FILE *fp = fopen("/proc/self/status", "r");
    if (fp) {
        char line[96];
        while (fgets(line, sizeof(line), fp)) {
            unsigned long kb = 0;
            if (sscanf(line, "VmRSS: %lu kB", &kb) == 1) {
                fclose(fp);
                atomic_store_explicit(&cached_kb, kb, memory_order_relaxed);
                atomic_store_explicit(&cached_at, now, memory_order_relaxed);
                return kb;
            }
        }
        fclose(fp);
    }
#endif
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) == 0) {
#if defined(__APPLE__)
        unsigned long kb = (unsigned long)(ru.ru_maxrss / 1024UL);
#else
        unsigned long kb = (unsigned long)ru.ru_maxrss;
#endif
        atomic_store_explicit(&cached_kb, kb, memory_order_relaxed);
        atomic_store_explicit(&cached_at, now, memory_order_relaxed);
        return kb;
    }
#endif
    return cached;
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
