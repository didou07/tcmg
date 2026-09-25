#ifndef TCMG_WEBIF_MACROS_H_
#define TCMG_WEBIF_MACROS_H_

#define PAGE_INIT(initial_sz) \
	int   bsz = (initial_sz), pos = 0; \
	char *buf = (char *)malloc(bsz); \
	if (!buf) { send_json_error(fd, 503, "Service Unavailable", "out of memory"); return; }

#define PAGE_SEND_AND_FREE(fd) \
	send_response((fd), 200, "OK", "text/html", buf, pos); \
	free(buf);

                                                                    
static inline void send_json_error(int fd, int code, const char *reason,
                                   const char *msg)
{
	char esc[384], e[512];
	json_escape(msg, esc, sizeof(esc));
	int  n = snprintf(e, sizeof(e), "{\"ok\":false,\"msg\":\"%s\"}", esc);
	send_response(fd, code, reason, "application/json", e, n);
}

static inline void send_json_ok(int fd, const char *msg)
{
	char esc[384], e[512];
	json_escape(msg, esc, sizeof(esc));
	int  n = snprintf(e, sizeof(e), "{\"ok\":true,\"msg\":\"%s\"}", esc);
	send_response(fd, 200, "OK", "application/json", e, n);
}

static inline void send_json_ok_raw(int fd, const char *json)
{
	send_response(fd, 200, "OK", "application/json", json, (int)strlen(json));
}

#endif
