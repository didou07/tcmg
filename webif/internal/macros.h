#ifndef TCMG_WEBIF_MACROS_H_
#define TCMG_WEBIF_MACROS_H_

#include "../../globals.h"

#define PAGE_INIT(initial_sz) \
	int   bsz = (initial_sz), pos = 0; \
	char *buf = (char *)malloc(bsz); \
	if (!buf) return;

#define PAGE_SEND_AND_FREE(fd) \
	send_response((fd), 200, "OK", "text/html", buf, pos); \
	free(buf);

static inline void send_json_error(int fd, int code, const char *reason,
                                   const char *msg)
{
	char e[256];
	int  n = snprintf(e, sizeof(e), "{\"ok\":false,\"msg\":\"%s\"}", msg);
	send_response(fd, code, reason, "application/json", e, n);
}

static inline void send_json_ok(int fd, const char *msg)
{
	char e[256];
	int  n = snprintf(e, sizeof(e), "{\"ok\":true,\"msg\":\"%s\"}", msg);
	send_response(fd, 200, "OK", "application/json", e, n);
}

static inline void send_json_ok_raw(int fd, const char *json)
{
	send_response(fd, 200, "OK", "application/json", json, (int)strlen(json));
}

#endif
