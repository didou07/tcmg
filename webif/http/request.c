#define MODULE_LOG_PREFIX "webif"
#include "../../globals.h"
#include "../internal/proto.h"

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
