#ifndef TCMG_WEBIF_TYPES_H_
#define TCMG_WEBIF_TYPES_H_

#include "constants.h"

typedef struct {
	char   token[WEB_SESSION_LEN + 1];
	time_t expires;
	time_t issued_at;
} s_session;

typedef struct {
	char  method[8];
	char  path[512];
	char  qs[512];
	char *body;
	int   body_len;
} s_http_req;

typedef struct {
	int64_t  cw_found;
	int64_t  cw_not;
	int64_t  ecm_total;
	double   hit_rate;
	int      nbans;
	int      naccounts;
	int      active_conns;
	time_t   uptime_s;
	char     uptime_str[32];
} S_SERVER_STATS;

typedef struct { const char *id; const char *href; const char *icon; const char *label; } NavItem;

#endif
