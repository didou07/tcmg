#define MODULE_LOG_PREFIX "webif"
#include "../service/service.h"
#include "../internal/proto.h"
#include "../../src/core/constants.h"
#include "../../src/core/utils.h"
#include "../../src/log/log.h"
#include <errno.h>
                                                                                
                                                                            
                                                                         
                                                                          
                                                                              
                                                                       
                        
typedef struct {
	char   user[CFGKEY_LEN];
	char   pass[CFGKEY_LEN];
	int    has_caid, has_max, has_en, has_exp;
	uint16_t caidv[MAX_CAIDS_PER_ACC + 1];
	int      ncaidv;
	int    maxc, en;
	int    has_as, as_en, as_sids, as_ecm, as_ecm_window_s, as_channel_timeout_s, as_switch_delay_s;
	char   groups[256];
	int32_t groupv[MAX_GROUPS_PER_ACC];
	int32_t ngroups;
	time_t exp;                                   
} acct_form;

static int get_field(const char *body, const char *key, char *out, size_t outsz)
{
	out[0] = '\0';
	char *v = form_get_alloc(body, key);
	if (!v) return 0;
	int too_long = (strlen(v) >= outsz);
	if (!too_long) tcmg_strlcpy(out, v, outsz);
	free(v);
	return too_long ? -1 : 0;
}

static int all_digits(const char *s, size_t maxlen)
{
	size_t n = strlen(s);
	if (!n || n > maxlen) return 0;
	for (; *s; s++) if (*s < '0' || *s > '9') return 0;
	return 1;
}

static int parse_uint_range(const char *s, size_t maxlen, int lo, int hi, int *out)
{
    if (!s || !out || !all_digits(s, maxlen)) return -1;
    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || errno == ERANGE || v < lo || v > hi) return -1;
    *out = (int)v;
    return 0;
}

                                                                                      
static int parse_date(const char *s, time_t *out)
{
	int y, m, d;
	char tail;
	if (sscanf(s, "%d-%d-%d%c", &y, &m, &d, &tail) != 3) return -1;
	if (y < 2000 || y > 2100 || m < 1 || m > 12 || d < 1 || d > 31) return -1;
	struct tm t;
	memset(&t, 0, sizeof(t));
	t.tm_year = y - 1900; t.tm_mon = m - 1; t.tm_mday = d;
	t.tm_isdst = -1;                                                                           
	time_t r = mktime(&t);
	if (r == (time_t)-1 || t.tm_mday != d || t.tm_mon != m - 1) return -1;
	*out = r;
	return 0;
}

                                                                
static void api_trim(char *s)
{
	size_t n = strlen(s);
	while (n && isspace((unsigned char)s[n - 1])) s[--n] = 0;
	char *p = s;
	while (*p && isspace((unsigned char)*p)) p++;
	if (p != s) memmove(s, p, strlen(p) + 1);
}

static int parse_groups(const char *s, int32_t out[MAX_GROUPS_PER_ACC], int32_t *n)
{
	char buf[256];
	*n = 0;
	if (!s[0]) { out[0] = 1; *n = 1; return 0; }
	tcmg_strlcpy(buf, s, sizeof(buf));
	char *save = NULL, *tok = strtok_r(buf, ",", &save);
	while (tok) {
		api_trim(tok);
		if (!tok[0] || *n >= MAX_GROUPS_PER_ACC) return -1;
		char *e = NULL;
		long v = strtol(tok, &e, 10);
		if (e == tok || *e || v < 1 || v > 65535) return -1;
		for (int i = 0; i < *n; i++) if (out[i] == (int32_t)v) return -1;
		out[(*n)++] = (int32_t)v;
		tok = strtok_r(NULL, ",", &save);
	}
	return *n > 0 ? 0 : -1;
}

static int parse_caid_list(const char *s, acct_form *f)
{
	char buf[128];
	int  only_zero = 1;
	f->ncaidv = 0;
	tcmg_strlcpy(buf, s, sizeof(buf));
	char *save = NULL, *tok = strtok_r(buf, ", ;\t", &save);
	while (tok) {
		size_t n = strlen(tok);
		if (n < 1 || n > 4) return -1;
		for (size_t i = 0; i < n; i++) if (!isxdigit((unsigned char)tok[i])) return -1;
		uint16_t c = (uint16_t)strtoul(tok, NULL, 16);
		if (c) {
			only_zero = 0;
			int dup = 0;
			for (int i = 0; i < f->ncaidv; i++) if (f->caidv[i] == c) dup = 1;
			if (!dup) {
				if (f->ncaidv >= MAX_CAIDS_PER_ACC + 1) return -2;
				f->caidv[f->ncaidv++] = c;
			}
		}
		tok = strtok_r(NULL, ", ;\t", &save);
	}
	if (only_zero || f->ncaidv == 0) { f->caidv[0] = 0; f->ncaidv = 1; }
	return 0;
}


static const char *parse_account_form(const char *body, acct_form *f)
{
	char caid_s[128], max_s[16], en_s[8], exp_s[24];
	char as_en_s[8], as_sids_s[8], as_ecm_s[16], as_ecm_window_s[8], as_channel_timeout_s[8], as_switch_delay_s[8];
	memset(f, 0, sizeof(*f));
	if (get_field(body, "user",    f->user, sizeof(f->user))    < 0) return "username too long (max 63 characters)";
	if (get_field(body, "pass",    f->pass, sizeof(f->pass))    < 0) return "password too long (max 63 characters)";
	if (get_field(body, "groups",  f->groups, sizeof(f->groups)) < 0) return "groups too long";
	if (get_field(body, "caid",    caid_s,  sizeof(caid_s))     < 0 ||
	    get_field(body, "maxconn", max_s,   sizeof(max_s))      < 0 ||
	    get_field(body, "enabled", en_s,    sizeof(en_s))       < 0 ||
	    get_field(body, "expiry",  exp_s,   sizeof(exp_s))      < 0 ||
	    get_field(body, "anti_share",    as_en_s,   sizeof(as_en_s))   < 0 ||
	    get_field(body, "as_max_sids",   as_sids_s, sizeof(as_sids_s)) < 0 ||
	    get_field(body, "as_max_ecm",as_ecm_s, sizeof(as_ecm_s)) < 0 ||
	    get_field(body, "as_ecm_window_s",as_ecm_window_s, sizeof(as_ecm_window_s)) < 0 ||
	    get_field(body, "as_channel_timeout_s",as_channel_timeout_s, sizeof(as_channel_timeout_s)) < 0 ||
	    get_field(body, "as_switch_delay_s",as_switch_delay_s, sizeof(as_switch_delay_s)) < 0) return "invalid field length";

	if (!f->user[0]) return "username required";
	if (!web_valid_text(f->user))
		return "username must not contain '#', control characters or leading/trailing spaces";
	if (f->pass[0] && !web_valid_text(f->pass))
		return "password must not contain '#', control characters or leading/trailing spaces";
	if (parse_groups(f->groups, f->groupv, &f->ngroups) < 0)
		return "groups must be comma-separated values from 1 to 65535";

	if (caid_s[0]) {
		int rc = parse_caid_list(caid_s, f);
		if (rc == -2) return "at most 9 CAIDs per user";
		if (rc < 0)   return "CAIDs must be hex values of 1-4 digits separated by commas, e.g. 0B00,0B01";
		f->has_caid = 1;
	}
	if (max_s[0]) {
		if (parse_uint_range(max_s, 4, 0, 9999, &f->maxc) < 0) return "max connections must be a whole number 0-9999";
		f->has_max = 1;
	}
	if (en_s[0]) {
		if (strcmp(en_s, "0") && strcmp(en_s, "1")) return "enabled must be 0 or 1";
		f->has_en = 1; f->en = en_s[0] == '1';
	}
	if (exp_s[0] && strcmp(exp_s, "0") != 0) {
		if (parse_date(exp_s, &f->exp) < 0) return "expiry must be a valid date YYYY-MM-DD";
	}
	f->has_exp = 1;

	if (as_en_s[0]) {
		if (strcmp(as_en_s, "0") && strcmp(as_en_s, "1")) return "anti_share must be 0 or 1";
		f->has_as = 1; f->as_en = as_en_s[0] == '1';
	}
	if (as_sids_s[0]) {
		if (parse_uint_range(as_sids_s, 2, 1, 32, &f->as_sids) < 0) return "max simultaneous channels must be 1-32";
	} else f->as_sids = 1;
	if (as_ecm_s[0]) {
		if (parse_uint_range(as_ecm_s, 6, 0, 100000, &f->as_ecm) < 0) return "max ECM must be 0-100000";
	} else f->as_ecm = 0;
	if (as_ecm_window_s[0]) {
		if (parse_uint_range(as_ecm_window_s, 4, 1, 3600, &f->as_ecm_window_s) < 0) return "ECM window must be 1-3600 seconds";
	} else f->as_ecm_window_s = 60;
	if (as_channel_timeout_s[0]) {
		if (parse_uint_range(as_channel_timeout_s, 4, 1, 3600, &f->as_channel_timeout_s) < 0) return "channel timeout must be 1-3600 seconds";
	} else f->as_channel_timeout_s = 15;
	if (as_switch_delay_s[0]) {
		if (parse_uint_range(as_switch_delay_s, 2, 0, 30, &f->as_switch_delay_s) < 0) return "channel switch delay must be 0-30 seconds";
	} else f->as_switch_delay_s = 1;

	return NULL;
}

                                                                                  

static void api_userstats_find_session(const S_WEBIF_CLIENT_VIEW *clients, int nclients, const char *user,
                                        int *active, const S_WEBIF_CLIENT_VIEW **best, long *idle_s)
{
    time_t now = time(NULL);
    int n = 0;
    const S_WEBIF_CLIENT_VIEW *pick = NULL;
    time_t last = 0;
    for (int i = 0; i < nclients; i++) {
        if (strcmp(clients[i].user, user) != 0) continue;
        n++;
        if (clients[i].last_activity >= last) {
            last = clients[i].last_activity;
            pick = &clients[i];
        }
    }
    if (active) *active = n;
    if (best) *best = pick;
    if (idle_s) *idle_s = (pick && pick->last_activity > 0) ? (long)(now - pick->last_activity) : -1;
    if (idle_s && *idle_s < 0 && pick) *idle_s = 0;
}

void send_api_userstats(int fd)
{
    int cap = webif_account_count();
    if (cap < 0) cap = 0;
    S_WEBIF_USER_STATS_VIEW *accounts = cap ? calloc((size_t)cap, sizeof(*accounts)) : NULL;
    S_WEBIF_CLIENT_VIEW clients[MAX_ACTIVE_CLIENTS];
    int naccounts = accounts ? webif_account_userstats_snapshot_all(accounts, (size_t)cap) : 0;
    int nclients = webif_client_snapshot_all(clients, MAX_ACTIVE_CLIENTS);
    if (cap > 0 && !accounts) {
        send_json_error(fd, 503, "Service Unavailable", "out of memory");
        return;
    }

    int bsz = 16384, pos = 0;
    char *buf = malloc((size_t)bsz);
    if (!buf) { free(accounts); send_json_error(fd, 503, "Service Unavailable", "out of memory"); return; }

    pos = buf_printf(&buf, &bsz, pos,
        "{\"ok\":true,\"active_connections\":%d,\"count\":%d,\"users\":[",
        webif_active_connection_count(), naccounts);

    for (int i = 0; i < naccounts; i++) {
        const S_WEBIF_USER_STATS_VIEW *a = &accounts[i];
        int active = 0;
        const S_WEBIF_CLIENT_VIEW *best = NULL;
        long idle_s = -1;
        api_userstats_find_session(clients, nclients, a->user, &active, &best, &idle_s);

        char eu[256], eip[128], eproto[64], echan[256], ecaids[256];
        json_escape(a->user, eu, sizeof(eu));
        json_escape(best ? best->ip : a->last_ip, eip, sizeof(eip));
        json_escape(best ? best->proto : "", eproto, sizeof(eproto));
        json_escape(best ? best->channel : "", echan, sizeof(echan));
        json_escape(a->caids, ecaids, sizeof(ecaids));
        uint32_t ipn = 0;
        {
            const char *ip_for_sort = best ? best->ip : a->last_ip;
            unsigned o[4];
            if (sscanf(ip_for_sort, "%u.%u.%u.%u", &o[0], &o[1], &o[2], &o[3]) == 4)
                ipn = ((o[0] & 255u) << 24) | ((o[1] & 255u) << 16) | ((o[2] & 255u) << 8) | (o[3] & 255u);
        }
        long long avg = a->cw_found > 0 ? (long long)(a->cw_time_total_ms / a->cw_found) : -1;
        unsigned caid = best ? best->caid : 0;
        unsigned sid = best ? best->sid : 0;
        time_t last_seen = a->last_seen;

        pos = buf_printf(&buf, &bsz, pos,
            "%s{\"user\":\"%s\",\"enabled\":%d,\"active\":%d,\"ok\":%lld,\"nok\":%lld,"
            "\"avg\":%lld,\"ip\":\"%s\",\"ipn\":%u,\"proto\":\"%s\",\"caid\":\"%04X\","
            "\"sid\":\"%04X\",\"channel\":\"%s\",\"idle\":%ld,\"last_seen\":%lld,\"caids\":\"%s\"}",
            i ? "," : "", eu, a->enabled ? 1 : 0, active,
            (long long)a->cw_found, (long long)a->cw_not, avg,
            eip, ipn, eproto, caid, sid, echan, idle_s, (long long)last_seen, ecaids);
    }
    pos = buf_printf(&buf, &bsz, pos, "]}");
    send_response(fd, 200, "OK", "application/json", buf, pos);
    free(buf);
    free(accounts);
}

void handle_user_toggle(int fd, const char *qs)
{
	char uname[CFGKEY_LEN] = "";
	get_param(qs, "user", uname, sizeof(uname));
	int enabled = -1;
	if (uname[0] && webif_account_toggle(uname, &enabled)) {
		tcmg_log("webif: user='%s' %s", uname, enabled ? "enabled" : "disabled");
		char j[64]; snprintf(j, sizeof(j), "{\"ok\":true,\"enabled\":%d}", enabled);
		send_json_ok_raw(fd, j); return;
	}
	if (!webif_account_get(uname, &(S_WEBIF_ACCOUNT_VIEW){0})) {
		send_json_error(fd, 404, "Not Found", "user not found"); return;
	}
	send_json_error(fd, 500, "Internal Error", "failed to save config");
}

void send_api_user_get(int fd, const char *qs)
{
	char uname[CFGKEY_LEN] = ""; get_param(qs, "user", uname, sizeof(uname));
	S_WEBIF_ACCOUNT_VIEW a; if (!webif_account_get(uname, &a)) { send_json_error(fd,404,"Not Found","not found"); return; }
	char expiry[24] = "0"; if (a.expirationdate > 0) { struct tm tm_s; localtime_r(&a.expirationdate,&tm_s); strftime(expiry,sizeof(expiry),"%Y-%m-%d",&tm_s); }
	char eu[256],ep[256],eg[512],ec[256]; json_escape(a.user,eu,sizeof(eu));json_escape(a.pass,ep,sizeof(ep));json_escape(a.groups,eg,sizeof(eg));json_escape(a.caids,ec,sizeof(ec));
	char out[2048]; int n=snprintf(out,sizeof(out),"{\"ok\":true,\"user\":\"%s\",\"pass\":\"%s\",\"groups\":\"%s\",\"caid\":\"%s\",\"max_connections\":%d,\"enabled\":%d,\"expiry\":\"%s\",\"anti_share\":%d,\"as_max_sids\":%d,\"as_max_ecm\":%d,\"as_ecm_window_s\":%d,\"as_channel_timeout_s\":%d,\"as_switch_delay_s\":%d}",eu,ep,eg,ec,a.max_connections,a.enabled,expiry,a.anti_share,a.as_max_sids,a.as_max_ecm,a.as_ecm_window_s,a.as_channel_timeout_s,a.as_switch_delay_s);
	send_response(fd,200,"OK","application/json",out,n);
}

static void account_form_to_edit(const acct_form *f, S_WEBIF_ACCOUNT_EDIT *e)
{
	memset(e,0,sizeof(*e)); tcmg_strlcpy(e->user,f->user,sizeof(e->user)); tcmg_strlcpy(e->pass,f->pass,sizeof(e->pass)); tcmg_strlcpy(e->groups,f->groups,sizeof(e->groups));
	e->has_caid=f->has_caid; e->ncaidv=f->ncaidv; memcpy(e->caidv,f->caidv,sizeof(e->caidv)); e->has_max_connections=f->has_max; e->max_connections=f->maxc; e->has_enabled=f->has_en; e->enabled=f->en; e->has_expiry=f->has_exp; e->expiry=f->exp; e->has_anti_share=f->has_as; e->anti_share=f->as_en; e->as_max_sids=f->as_sids; e->as_max_ecm=f->as_ecm; e->as_ecm_window_s=f->as_ecm_window_s; e->as_channel_timeout_s=f->as_channel_timeout_s; e->as_switch_delay_s=f->as_switch_delay_s; e->ngroups=f->ngroups; memcpy(e->groupv,f->groupv,sizeof(e->groupv));
}

void handle_user_save(int fd, const char *post_body)
{
	acct_form f; const char *err=parse_account_form(post_body,&f); if(err){send_json_error(fd,400,"Bad Request",err);return;}
	S_WEBIF_ACCOUNT_EDIT e; account_form_to_edit(&f,&e);
	if(!webif_account_save(&e)){ if(!webif_account_get(f.user,&(S_WEBIF_ACCOUNT_VIEW){0})) send_json_error(fd,404,"Not Found","user not found"); else send_json_error(fd,500,"Internal Error","failed to save config"); return; }
	 tcmg_log("webif: user='%s' updated",f.user); send_json_ok(fd,"ok");
}

void handle_user_delete(int fd, const char *qs)
{
	char uname[CFGKEY_LEN]=""; get_param(qs,"user",uname,sizeof(uname)); if(!uname[0]){send_json_error(fd,400,"Bad Request","missing user");return;}
	if(!webif_account_delete(uname)){ if(!webif_account_get(uname,&(S_WEBIF_ACCOUNT_VIEW){0})) send_json_error(fd,404,"Not Found","user not found"); else send_json_error(fd,500,"Internal Error","failed to save config"); return; }
	tcmg_log("webif: user='%s' deleted",uname); send_json_ok(fd,"ok");
}

void handle_user_add(int fd, const char *post_body)
{
	acct_form f; const char *err=parse_account_form(post_body,&f); if(err){send_json_error(fd,400,"Bad Request",err);return;}
	S_WEBIF_ACCOUNT_EDIT e; account_form_to_edit(&f,&e); int status=500;
	if(!webif_account_add(&e,&status)){send_json_error(fd,status==409?409:500,status==409?"Conflict":"Internal Error",status==409?"username already exists":"failed to save config");return;}
	tcmg_log("webif: user='%s' added",f.user); send_json_ok(fd,"ok");
}

void handle_user_resetstats(int fd, const char *qs)
{
	char uname[CFGKEY_LEN]=""; get_param(qs,"user",uname,sizeof(uname)); if(!uname[0]){send_json_error(fd,400,"Bad Request","missing user");return;}
	if(!webif_account_reset_stats(uname)){send_json_error(fd,404,"Not Found","user not found");return;}
	tcmg_log("webif: stats reset for user='%s'",uname); send_json_ok(fd,"ok");
}
