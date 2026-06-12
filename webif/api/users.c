#define MODULE_LOG_PREFIX "webif"
#include "../../globals.h"
#include "../internal/proto.h"

void handle_user_toggle(int fd, const char *qs)
{
	char uname[CFGKEY_LEN] = "";
	get_param(qs, "user", uname, sizeof(uname));

	int enabled = -1;
	if (uname[0]) {
		pthread_rwlock_wrlock(&g_cfg.acc_lock);
		for (S_ACCOUNT *a = g_cfg.accounts; a; a = a->next) {
			if (strcmp(a->user, uname) == 0) {
				a->enabled = !a->enabled;
				enabled = a->enabled;
				break;
			}
		}
		pthread_rwlock_unlock(&g_cfg.acc_lock);
	}
	if (enabled < 0) {
		send_json_error(fd, 404, "Not Found", "user not found");
		return;
	}
	if (!cfg_save(&g_cfg)) {
		send_json_error(fd, 500, "Internal Error", "failed to save config");
		return;
	}
	if (!enabled)
		client_kill_by_user(uname);
	tcmg_log("webif: user='%s' %s", uname, enabled ? "enabled" : "disabled");
	char j[64];
	snprintf(j, sizeof(j), "{\"ok\":true,\"enabled\":%d}", enabled);
	send_json_ok_raw(fd, j);
}

void send_api_user_get(int fd, const char *qs)
{
	char uname[CFGKEY_LEN] = "";
	get_param(qs, "user", uname, sizeof(uname));

	int   bsz = 512, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	S_ACCOUNT *a = g_cfg.accounts;
	while (a && strcmp(a->user, uname) != 0) a = a->next;

	if (!a) {
		pthread_rwlock_unlock(&g_cfg.acc_lock);
		free(buf);
		send_json_error(fd, 404, "Not Found", "not found");
		return;
	}
	char expiry[24] = "0";
	if (a->expirationdate > 0) {
		struct tm tm_s;
		localtime_r(&a->expirationdate, &tm_s);
		strftime(expiry, sizeof(expiry), "%Y-%m-%d", &tm_s);
	}
	char esc_pass[CFGKEY_LEN * 2];
	json_escape(a->pass, esc_pass, sizeof(esc_pass));

	pos = buf_printf(&buf, &bsz, pos,
		"{\"ok\":true,\"user\":\"%s\","
		"\"pass\":\"%s\","
		"\"caid\":\"%04X\",\"max_connections\":%d,"
		"\"enabled\":%d,\"expiry\":\"%s\"}",
		a->user, esc_pass, a->caid, (int)a->max_connections,
		(int)a->enabled, expiry);
	pthread_rwlock_unlock(&g_cfg.acc_lock);
	send_response(fd, 200, "OK", "application/json", buf, pos);
	free(buf);
}

void handle_user_save(int fd, const char *post_body)
{
	char uname[CFGKEY_LEN]="", pass[CFGKEY_LEN]="";
	char caid_s[8]="", maxconn_s[16]="", enabled_s[4]="", expiry_s[16]="";
	form_get(post_body, "user",    uname,     sizeof(uname));
	form_get(post_body, "pass",    pass,      sizeof(pass));
	form_get(post_body, "caid",    caid_s,    sizeof(caid_s));
	form_get(post_body, "maxconn", maxconn_s, sizeof(maxconn_s));
	form_get(post_body, "enabled", enabled_s, sizeof(enabled_s));
	form_get(post_body, "expiry",  expiry_s,  sizeof(expiry_s));

	if (!uname[0]) {
		send_json_error(fd, 400, "Bad Request", "missing user");
		return;
	}

	pthread_rwlock_wrlock(&g_cfg.acc_lock);
	S_ACCOUNT *a = g_cfg.accounts;
	while (a && strcmp(a->user, uname) != 0) a = a->next;
	if (!a) {
		pthread_rwlock_unlock(&g_cfg.acc_lock);
		send_json_error(fd, 404, "Not Found", "user not found");
		return;
	}

	tcmg_strlcpy(a->pass, pass, sizeof(a->pass));
	if (caid_s[0])     a->caid            = (uint16_t)strtol(caid_s, NULL, 16);
	if (maxconn_s[0])  a->max_connections = atoi(maxconn_s);
	a->enabled = atoi(enabled_s);

	if (expiry_s[0] && strcmp(expiry_s, "0") != 0) {
		struct tm tm_s = {0};
		if (sscanf(expiry_s, "%d-%d-%d",
		           &tm_s.tm_year, &tm_s.tm_mon, &tm_s.tm_mday) == 3) {
			tm_s.tm_year -= 1900;
			tm_s.tm_mon  -= 1;
			a->expirationdate = mktime(&tm_s);
		}
	} else {
		a->expirationdate = 0;
	}
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	if (!cfg_save(&g_cfg)) {
		send_json_error(fd, 500, "Internal Error", "failed to save config");
		return;
	}
	if (!atoi(enabled_s))
		client_kill_by_user(uname);
	tcmg_log("webif: user='%s' updated", uname);
	send_json_ok(fd, "ok");
}

void handle_user_delete(int fd, const char *qs)
{
	char uname[CFGKEY_LEN] = "";
	get_param(qs, "user", uname, sizeof(uname));

	if (!uname[0]) {
		send_json_error(fd, 400, "Bad Request", "missing user");
		return;
	}

	int found = 0;
	pthread_rwlock_wrlock(&g_cfg.acc_lock);
	S_ACCOUNT **pp = &g_cfg.accounts;
	while (*pp) {
		if (strcmp((*pp)->user, uname) == 0) {
			S_ACCOUNT *del = *pp;
			*pp = del->next;
			free(del);
			g_cfg.naccounts--;
			found = 1;
			break;
		}
		pp = &(*pp)->next;
	}
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	if (!found) {
		send_json_error(fd, 404, "Not Found", "user not found");
		return;
	}
	if (!cfg_save(&g_cfg)) {
		send_json_error(fd, 500, "Internal Error", "failed to save config");
		return;
	}
	tcmg_log("webif: user='%s' deleted", uname);
	send_json_ok(fd, "ok");
}

void handle_user_add(int fd, const char *post_body)
{
	char uname[CFGKEY_LEN]="", pass[CFGKEY_LEN]="";
	char caid_s[8]="", maxconn_s[16]="", enabled_s[4]="", expiry_s[16]="";
	form_get(post_body, "user",    uname,     sizeof(uname));
	form_get(post_body, "pass",    pass,      sizeof(pass));
	form_get(post_body, "caid",    caid_s,    sizeof(caid_s));
	form_get(post_body, "maxconn", maxconn_s, sizeof(maxconn_s));
	form_get(post_body, "enabled", enabled_s, sizeof(enabled_s));
	form_get(post_body, "expiry",  expiry_s,  sizeof(expiry_s));

	if (!uname[0]) {
		send_json_error(fd, 400, "Bad Request", "username required");
		return;
	}

	pthread_rwlock_wrlock(&g_cfg.acc_lock);

	for (S_ACCOUNT *a = g_cfg.accounts; a; a = a->next) {
		if (strcmp(a->user, uname) == 0) {
			pthread_rwlock_unlock(&g_cfg.acc_lock);
			send_json_error(fd, 409, "Conflict", "username already exists");
			return;
		}
	}

	S_ACCOUNT *a = cfg_account_new(&g_cfg);
	if (!a) {
		pthread_rwlock_unlock(&g_cfg.acc_lock);
		send_json_error(fd, 500, "Internal Error", "out of memory");
		return;
	}

	tcmg_strlcpy(a->user, uname, sizeof(a->user));
	if (pass[0])      tcmg_strlcpy(a->pass, pass, sizeof(a->pass));
	if (caid_s[0])    a->caid            = (uint16_t)strtol(caid_s, NULL, 16);
	if (maxconn_s[0]) a->max_connections = atoi(maxconn_s);
	a->enabled = enabled_s[0] ? atoi(enabled_s) : 1;

	if (expiry_s[0] && strcmp(expiry_s, "0") != 0) {
		struct tm tm_s = {0};
		if (sscanf(expiry_s, "%d-%d-%d",
		           &tm_s.tm_year, &tm_s.tm_mon, &tm_s.tm_mday) == 3) {
			tm_s.tm_year -= 1900;
			tm_s.tm_mon  -= 1;
			a->expirationdate = mktime(&tm_s);
		}
	}
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	if (!cfg_save(&g_cfg)) {
		send_json_error(fd, 500, "Internal Error", "failed to save config");
		return;
	}
	tcmg_log("webif: user='%s' added", uname);
	send_json_ok(fd, "ok");
}

void handle_user_resetstats(int fd, const char *qs)
{
	char uname[CFGKEY_LEN] = "";
	get_param(qs, "user", uname, sizeof(uname));

	if (!uname[0]) {
		send_json_error(fd, 400, "Bad Request", "missing user");
		return;
	}

	pthread_rwlock_wrlock(&g_cfg.acc_lock);
	S_ACCOUNT *a = g_cfg.accounts;
	while (a && strcmp(a->user, uname) != 0) a = a->next;
	if (!a) {
		pthread_rwlock_unlock(&g_cfg.acc_lock);
		send_json_error(fd, 404, "Not Found", "user not found");
		return;
	}
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
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	tcmg_log("webif: stats reset for user='%s'", uname);
	send_json_ok(fd, "ok");
}
