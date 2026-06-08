#define MODULE_LOG_PREFIX "webif"
#include "../globals.h"
#include "webif-internal.h"

void send_api_status(int fd)
{
	int   bsz = 16384, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	S_SERVER_STATS st  = collect_stats();
	time_t         now = time(NULL);

	pos = buf_printf(&buf, &bsz, pos,
		"{"
		"\"version\":\"%s\","
		"\"build\":\"%s\","
		"\"uptime_s\":%ld,"
		"\"uptime_str\":\"%s\","
		"\"newcamd_port\":%d,"
		"\"active_connections\":%d,"
		"\"accounts\":%d,"
		"\"banned_ips\":%d,"
		"\"cw_found\":%lld,"
		"\"cw_not\":%lld,"
		"\"ecm_total\":%lld,"
		"\"hit_rate_pct\":%.1f,"
		"\"debug_mask\":%u,"
		"\"clients\":[",
		TCMG_VERSION, TCMG_BUILD_TIME,
		(long)st.uptime_s, st.uptime_str,
		g_cfg.newcamd_port, st.active_conns,
		st.naccounts, st.nbans,
		(long long)st.cw_found, (long long)st.cw_not, (long long)st.ecm_total,
		st.hit_rate, g_dblevel);

	pthread_mutex_lock(&g_clients_mtx);
	bool first = true;
	for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++) {
		S_CLIENT *cl = g_clients[i];
		if (!cl || !cl->account) continue;
		char conn_str[32], idle_str[32];
		format_uptime(now - cl->connect_time,         conn_str, sizeof(conn_str));
		format_uptime(now - cl->account->last_seen,   idle_str, sizeof(idle_str));
		pos = buf_printf(&buf, &bsz, pos,
			"%s{"
			"\"user\":\"%s\","
			"\"ip\":\"%s\","
			"\"caid\":\"%04X\","
			"\"sid\":\"%04X\","
			"\"channel\":\"%s\","
			"\"connected\":\"%s\","
			"\"idle\":\"%s\","
			"\"thread_id\":%u"
			"}",
			first ? "" : ",",
			cl->user, cl->ip,
			cl->last_caid, cl->last_srvid,
			cl->last_channel[0] ? cl->last_channel : "",
			conn_str, idle_str,
			cl->thread_id);
		first = false;
	}
	pthread_mutex_unlock(&g_clients_mtx);

	pos = buf_printf(&buf, &bsz, pos, "]}");
	send_response(fd, 200, "OK", "application/json", buf, pos);
	free(buf);
}

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
		const char *e = "{\"ok\":false,\"msg\":\"user not found\"}";
		send_response(fd, 404, "Not Found", "application/json", e, (int)strlen(e));
		return;
	}
	cfg_save(&g_cfg);
	tcmg_log("webif: user='%s' %s", uname, enabled ? "enabled" : "disabled");
	char j[64];
	snprintf(j, sizeof(j), "{\"ok\":true,\"enabled\":%d}", enabled);
	send_response(fd, 200, "OK", "application/json", j, (int)strlen(j));
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
		const char *e = "{\"ok\":false,\"msg\":\"not found\"}";
		send_response(fd, 404, "Not Found", "application/json", e, (int)strlen(e));
		return;
	}
	char expiry[24] = "0";
	if (a->expirationdate > 0) {
		struct tm tm_s;
		localtime_r(&a->expirationdate, &tm_s);
		strftime(expiry, sizeof(expiry), "%Y-%m-%d", &tm_s);
	}

	pos = buf_printf(&buf, &bsz, pos,
		"{\"ok\":true,\"user\":\"%s\","
		"\"caid\":\"%04X\",\"max_connections\":%d,"
		"\"enabled\":%d,\"expiry\":\"%s\"}",
		a->user, a->caid, (int)a->max_connections,
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
		const char *e = "{\"ok\":false,\"msg\":\"missing user\"}";
		send_response(fd, 400, "Bad Request", "application/json", e, (int)strlen(e));
		return;
	}

	pthread_rwlock_wrlock(&g_cfg.acc_lock);
	S_ACCOUNT *a = g_cfg.accounts;
	while (a && strcmp(a->user, uname) != 0) a = a->next;
	if (!a) {
		pthread_rwlock_unlock(&g_cfg.acc_lock);
		const char *e = "{\"ok\":false,\"msg\":\"user not found\"}";
		send_response(fd, 404, "Not Found", "application/json", e, (int)strlen(e));
		return;
	}

	if (pass[0])       tcmg_strlcpy(a->pass, pass, sizeof(a->pass));
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

	cfg_save(&g_cfg);
	tcmg_log("webif: user='%s' updated", uname);
	const char *j = "{\"ok\":true}";
	send_response(fd, 200, "OK", "application/json", j, (int)strlen(j));
}

void handle_user_delete(int fd, const char *qs)
{
	char uname[CFGKEY_LEN] = "";
	get_param(qs, "user", uname, sizeof(uname));

	if (!uname[0]) {
		const char *e = "{\"ok\":false,\"msg\":\"missing user\"}";
		send_response(fd, 400, "Bad Request", "application/json", e, (int)strlen(e));
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
		const char *e = "{\"ok\":false,\"msg\":\"user not found\"}";
		send_response(fd, 404, "Not Found", "application/json", e, (int)strlen(e));
		return;
	}
	cfg_save(&g_cfg);
	tcmg_log("webif: user='%s' deleted", uname);
	const char *j = "{\"ok\":true}";
	send_response(fd, 200, "OK", "application/json", j, (int)strlen(j));
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
		const char *e = "{\"ok\":false,\"msg\":\"username required\"}";
		send_response(fd, 400, "Bad Request", "application/json", e, (int)strlen(e));
		return;
	}

	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	for (S_ACCOUNT *a = g_cfg.accounts; a; a = a->next) {
		if (strcmp(a->user, uname) == 0) {
			pthread_rwlock_unlock(&g_cfg.acc_lock);
			const char *e = "{\"ok\":false,\"msg\":\"username already exists\"}";
			send_response(fd, 409, "Conflict", "application/json", e, (int)strlen(e));
			return;
		}
	}
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	pthread_rwlock_wrlock(&g_cfg.acc_lock);
	S_ACCOUNT *a = cfg_account_new(&g_cfg);
	if (!a) {
		pthread_rwlock_unlock(&g_cfg.acc_lock);
		const char *e = "{\"ok\":false,\"msg\":\"out of memory\"}";
		send_response(fd, 500, "Internal Error", "application/json", e, (int)strlen(e));
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

	cfg_save(&g_cfg);
	tcmg_log("webif: user='%s' added", uname);
	const char *j = "{\"ok\":true}";
	send_response(fd, 200, "OK", "application/json", j, (int)strlen(j));
}

void handle_user_resetstats(int fd, const char *qs)
{
	char uname[CFGKEY_LEN] = "";
	get_param(qs, "user", uname, sizeof(uname));

	if (!uname[0]) {
		const char *e = "{\"ok\":false,\"msg\":\"missing user\"}";
		send_response(fd, 400, "Bad Request", "application/json", e, (int)strlen(e));
		return;
	}

	pthread_rwlock_wrlock(&g_cfg.acc_lock);
	S_ACCOUNT *a = g_cfg.accounts;
	while (a && strcmp(a->user, uname) != 0) a = a->next;
	if (!a) {
		pthread_rwlock_unlock(&g_cfg.acc_lock);
		const char *e = "{\"ok\":false,\"msg\":\"user not found\"}";
		send_response(fd, 404, "Not Found", "application/json", e, (int)strlen(e));
		return;
	}
	a->ecm_total        = 0;
	a->cw_found         = 0;
	a->cw_not           = 0;
	a->cw_time_total_ms = 0;
	a->cw_time_min_ms   = 0;
	a->cw_time_max_ms   = 0;
	a->first_login      = 0;
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	tcmg_log("webif: stats reset for user='%s'", uname);
	const char *j = "{\"ok\":true}";
	send_response(fd, 200, "OK", "application/json", j, (int)strlen(j));
}
void handle_srvid2_save(int fd, const char *post_body)
{
	char newcontent[32768] = "";
	form_get(post_body, "srvid2", newcontent, sizeof(newcontent));
	if (!newcontent[0]) {
		const char *e = "<html><body><h1>Empty srvid2 rejected</h1></body></html>";
		send_response(fd, 400, "Bad Request", "text/html", e, (int)strlen(e));
		return;
	}

	char path[CFGPATH_LEN];
	tcmg_build_path(path, sizeof(path), g_cfgdir, TCMG_SRVID_FILE);

	FILE *fp = fopen(path, "w");
	if (!fp) {
		const char *e = "<html><body><h1>Cannot write srvid2</h1></body></html>";
		send_response(fd, 500, "Internal Error", "text/html", e, (int)strlen(e));
		return;
	}
	fputs(newcontent, fp);
	fclose(fp);
	int n = srvid_load(path);
	tcmg_log("webif: srvid2 saved entries=%d reloaded", n);
	send_redirect(fd, "/config");
}

