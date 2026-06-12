#define MODULE_LOG_PREFIX "webif"
#include "../../globals.h"
#include "../internal/proto.h"

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

void send_api_config_get(int fd)
{
	int   bsz = 2048, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	char key_hex[29];
	for (int i = 0; i < 14; i++)
		snprintf(key_hex + i * 2, 3, "%02X", g_cfg.newcamd_key[i]);
	key_hex[28] = '\0';

	char esc_bindaddr[256], esc_logfile[512], esc_user[256], esc_webif_bind[256];

	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	json_escape(g_cfg.newcamd_bindaddr, esc_bindaddr,  sizeof(esc_bindaddr));
	json_escape(g_cfg.logfile,          esc_logfile,   sizeof(esc_logfile));
	json_escape(g_cfg.webif_user,       esc_user,      sizeof(esc_user));
	json_escape(g_cfg.webif_bindaddr,   esc_webif_bind,sizeof(esc_webif_bind));
	pos = buf_printf(&buf, &bsz, pos,
		"{"
		"\"newcamd_port\":%d,"
		"\"newcamd_bindaddr\":\"%s\","
		"\"newcamd_key\":\"%s\","
		"\"newcamd_keepalive\":%d,"
		"\"newcamd_mgclient\":%d,"
		"\"cccam_port\":%d,"
		"\"sock_timeout\":%d,"
		"\"ecm_log\":%d,"
		"\"logfile\":\"%s\","
		"\"webif_port\":%d,"
		"\"webif_refresh\":%d,"
		"\"webif_user\":\"%s\","
		"\"webif_bindaddr\":\"%s\""
		"}",
		g_cfg.newcamd_port,
		esc_bindaddr,
		key_hex,
		(int)g_cfg.newcamd_keepalive,
		(int)g_cfg.newcamd_mgclient,
		g_cfg.cccam_port,
		g_cfg.sock_timeout,
		(int)g_cfg.ecm_log,
		esc_logfile,
		g_cfg.webif_port,
		g_cfg.webif_refresh,
		esc_user,
		esc_webif_bind);
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	send_response(fd, 200, "OK", "application/json", buf, pos);
	free(buf);
}

void handle_api_config_save(int fd, const char *post_body)
{
	char newcamd_port_s[8]="", newcamd_bindaddr[MAXIPLEN]="";
	char newcamd_key_s[30]="", keepalive_s[4]="", mgclient_s[4]="";
	char cccam_port_s[8]="", sock_timeout_s[8]="", ecm_log_s[4]="";
	char logfile[CFGPATH_LEN]="";
	char webif_port_s[8]="", webif_refresh_s[8]="";
	char webif_user[CFGKEY_LEN]="", webif_pass[CFGKEY_LEN]="";
	char webif_bindaddr[MAXIPLEN]="";

	form_get(post_body, "newcamd_port",     newcamd_port_s,   sizeof(newcamd_port_s));
	form_get(post_body, "newcamd_bindaddr", newcamd_bindaddr, sizeof(newcamd_bindaddr));
	form_get(post_body, "newcamd_key",      newcamd_key_s,    sizeof(newcamd_key_s));
	form_get(post_body, "newcamd_keepalive",keepalive_s,      sizeof(keepalive_s));
	form_get(post_body, "newcamd_mgclient", mgclient_s,       sizeof(mgclient_s));
	form_get(post_body, "cccam_port",       cccam_port_s,     sizeof(cccam_port_s));
	form_get(post_body, "sock_timeout",     sock_timeout_s,   sizeof(sock_timeout_s));
	form_get(post_body, "ecm_log",          ecm_log_s,        sizeof(ecm_log_s));
	form_get(post_body, "logfile",          logfile,          sizeof(logfile));
	form_get(post_body, "webif_port",       webif_port_s,     sizeof(webif_port_s));
	form_get(post_body, "webif_refresh",    webif_refresh_s,  sizeof(webif_refresh_s));
	form_get(post_body, "webif_user",       webif_user,       sizeof(webif_user));
	form_get(post_body, "webif_pass",       webif_pass,       sizeof(webif_pass));
	form_get(post_body, "webif_bindaddr",   webif_bindaddr,   sizeof(webif_bindaddr));

	pthread_rwlock_wrlock(&g_cfg.acc_lock);

	if (newcamd_port_s[0]) {
		int p = atoi(newcamd_port_s);
		if (p > 0 && p < 65536) g_cfg.newcamd_port = p;
	}
	tcmg_strlcpy(g_cfg.newcamd_bindaddr, newcamd_bindaddr, sizeof(g_cfg.newcamd_bindaddr));

	if (newcamd_key_s[0] && strlen(newcamd_key_s) >= 28) {
		for (int i = 0; i < 14; i++) {
			char byte_s[3] = { newcamd_key_s[i*2], newcamd_key_s[i*2+1], '\0' };
			g_cfg.newcamd_key[i] = (uint8_t)strtol(byte_s, NULL, 16);
		}
	}
	if (keepalive_s[0]) g_cfg.newcamd_keepalive = (int8_t)atoi(keepalive_s);
	if (mgclient_s[0])  g_cfg.newcamd_mgclient  = (int8_t)atoi(mgclient_s);
	if (cccam_port_s[0]) {
		int p = atoi(cccam_port_s);
		if (p >= 0 && p < 65536) g_cfg.cccam_port = p;
	}
	if (sock_timeout_s[0]) {
		int t = atoi(sock_timeout_s);
		if (t >= 5 && t <= 600) g_cfg.sock_timeout = t;
	}
	if (ecm_log_s[0])       g_cfg.ecm_log       = (int8_t)atoi(ecm_log_s);
	if (logfile[0])         tcmg_strlcpy(g_cfg.logfile, logfile, sizeof(g_cfg.logfile));
	if (webif_port_s[0]) {
		int p = atoi(webif_port_s);
		if (p > 0 && p < 65536) g_cfg.webif_port = p;
	}
	if (webif_refresh_s[0]) {
		int r = atoi(webif_refresh_s);
		if (r >= 0 && r <= 3600) g_cfg.webif_refresh = r;
	}
	if (webif_user[0]) tcmg_strlcpy(g_cfg.webif_user, webif_user, sizeof(g_cfg.webif_user));
	tcmg_strlcpy(g_cfg.webif_pass, webif_pass, sizeof(g_cfg.webif_pass));
	tcmg_strlcpy(g_cfg.webif_bindaddr, webif_bindaddr, sizeof(g_cfg.webif_bindaddr));

	pthread_rwlock_unlock(&g_cfg.acc_lock);

	if (!cfg_save(&g_cfg)) {
		send_json_error(fd, 500, "Internal Error", "failed to write config file");
		return;
	}
	g_reload_cfg = 1;
	tcmg_log("%s", "webif: config updated via form -- reload triggered");
	send_json_ok(fd, "ok");
}

void handle_api_file_save(int fd, const char *post_body)
{
	char file_s[8] = "", content[32768] = "";
	form_get(post_body, "file",    file_s,  sizeof(file_s));
	form_get(post_body, "content", content, sizeof(content));

	int is_conf = (strcmp(file_s, "conf") == 0);
	int is_srv  = (strcmp(file_s, "srv")  == 0);

	if (!is_conf && !is_srv) {
		send_json_error(fd, 400, "Bad Request", "invalid file");
		return;
	}
	if (!content[0]) {
		send_json_error(fd, 400, "Bad Request", "empty content rejected");
		return;
	}

	char path[CFGPATH_LEN];
	if (is_conf)
		tcmg_build_path(path, sizeof(path), g_cfgdir, TCMG_CFG_FILE);
	else
		tcmg_build_path(path, sizeof(path), g_cfgdir, TCMG_SRVID_FILE);

	if (is_conf) {
		char tmppath[CFGPATH_LEN + 4];
		tcmg_build_path(tmppath, sizeof(tmppath), g_cfgdir, TCMG_CFG_FILE ".tmp");
		FILE *fp = fopen(tmppath, "w");
		if (!fp) {
			send_json_error(fd, 500, "Internal Error", "cannot write temp file");
			return;
		}
		fputs(content, fp);
		fclose(fp);
		S_CONFIG parsed;
		memset(&parsed, 0, sizeof(parsed));
		pthread_rwlock_init(&parsed.acc_lock, NULL);
		pthread_mutex_init(&parsed.ban_lock, NULL);
		int ok = cfg_load(tmppath, &parsed);
		remove(tmppath);
		cfg_accounts_free(&parsed);
		pthread_rwlock_destroy(&parsed.acc_lock);
		pthread_mutex_destroy(&parsed.ban_lock);
		if (!ok) {
			send_json_error(fd, 400, "Bad Request", "config parse error");
			return;
		}
	}

	FILE *fp = fopen(path, "w");
	if (!fp) {
		send_json_error(fd, 500, "Internal Error", "cannot write file");
		return;
	}
	fputs(content, fp);
	fclose(fp);

	if (is_conf) {
		g_reload_cfg = 1;
		tcmg_log("%s", "webif: tcmg.conf saved via api -- reload triggered");
	} else {
		int n = srvid_load(path);
		tcmg_log("webif: srvid2 saved entries=%d reloaded", n);
	}

	send_json_ok(fd, "ok");
}
