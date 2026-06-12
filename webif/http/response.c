#define MODULE_LOG_PREFIX "webif"
#include "../../globals.h"
#include "../internal/proto.h"

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
