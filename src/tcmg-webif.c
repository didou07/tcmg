#define MODULE_LOG_PREFIX "webif"
#include "tcmg-globals.h"
#include "tcmg-crypto.h"
#include "tcmg-log.h"
#include "tcmg-webif-internal.h"

volatile int8_t s_webif_running = 0;
int             s_webif_sock    = -1;

static pthread_t s_webif_tid;

typedef struct { int fd; char ip[MAXIPLEN]; } s_conn_arg;

static void *conn_thread(void *arg)
{
	s_conn_arg *c = (s_conn_arg *)arg;
	handle_request(c->fd, c->ip);
	close(c->fd);
	free(c);
	return NULL;
}

static const char *extract_header_value(const char *raw, const char *name,
                                         char *buf, int bufsz)
{
	buf[0] = '\0';
	char search[64];
	snprintf(search, sizeof(search), "\r\n%s:", name);
	const char *p = strstr(raw, search);
	if (!p) {
		snprintf(search, sizeof(search), "\n%s:", name);
		p = strstr(raw, search);
		if (!p) return NULL;
	}
	p = strchr(p, ':');
	if (!p) return NULL;
	p++;
	while (*p == ' ' || *p == '\t') p++;
	int i = 0;
	while (i < bufsz - 1 && *p && *p != '\r' && *p != '\n')
		buf[i++] = *p++;
	buf[i] = '\0';
	return buf;
}

static int request_is_authed(const char *raw, char *sess_tok_out)
{
	sess_tok_out[0] = '\0';

	if (!g_cfg.webif_user[0] && !g_cfg.webif_pass[0]) return 1;

	char cookie_hdr[512] = "";
	if (extract_header_value(raw, "Cookie", cookie_hdr, sizeof(cookie_hdr))) {
		char tok[WEB_SESSION_LEN + 1];
		const char *sess = cookie_get_session(cookie_hdr, tok, sizeof(tok));
		if (sess && session_check(sess)) {
			tcmg_strlcpy(sess_tok_out, sess, WEB_SESSION_LEN + 1);
			return 1;
		}
	}

	char auth_hdr[256] = "";
	if (extract_header_value(raw, "Authorization", auth_hdr, sizeof(auth_hdr))) {
		if (check_auth(auth_hdr)) return 1;
	}

	return 0;
}

static void send_json(int fd, const char *json)
{
	send_response(fd, 200, "OK", "application/json", json, (int)strlen(json));
}

static void send_json_401(int fd)
{
	static const char body[] = "{\"ok\":false,\"msg\":\"unauthorized\"}";
	send_response(fd, 401, "Unauthorized", "application/json",
	              body, (int)sizeof(body) - 1);
}

void handle_request(int fd, const char *client_ip)
{
	char *raw = (char *)malloc(WEB_BUF_SIZE);
	if (!raw) return;
	int  rlen = 0;
	net_set_timeout(fd, WEB_READ_TIMEOUT_S);

	while (rlen < WEB_BUF_SIZE - 1) {
		int n = (int)recv(fd, RECV_CAST(raw + rlen),
		                  (size_t)(WEB_BUF_SIZE - 1 - rlen), 0);
		if (n <= 0) break;
		rlen += n;
		raw[rlen] = '\0';
		if (strstr(raw, "\r\n\r\n")) break;
	}
	if (rlen < 10) { free(raw); return; }
	raw[rlen] = '\0';

	s_http_req req;
	if (!req_parse(&req, fd, raw, rlen)) { free(raw); return; }

	if (strcmp(req.path, "/logpoll") != 0)
		tcmg_log_dbg(D_WEBIF, "%s %s%s%s", req.method, req.path,
		             req.qs[0] ? "?" : "", req.qs);

	char sess_tok[WEB_SESSION_LEN + 1];
	int  authed = request_is_authed(raw, sess_tok);
	free(raw); raw = NULL;

	const char *p  = req.path;
	const char *qs = req.qs;

	/* ── POST /login  (always reachable) ── */
	if (strcmp(p, "/login") == 0 && strcmp(req.method, "POST") == 0) {
		char u[CFGKEY_LEN] = {0}, pw[CFGKEY_LEN] = {0};
		form_get(req.body, "u",  u,  sizeof(u));
		form_get(req.body, "p", pw, sizeof(pw));
		if (check_credentials(u, pw)) {
			char token[WEB_SESSION_LEN + 1];
			session_create(token);
			tcmg_log_dbg(D_WEBIF, "login OK for '%s' from %s", u, client_ip);
			send_redirect_with_cookie(fd, "/status", token);
		} else {
			tcmg_log("login FAIL for '%s' from %s", u, client_ip);
			send_login_page(fd, 1);
		}
		req_free(&req);
		return;
	}

	/* ── GET /logout  (always reachable) ── */
	if (strcmp(p, "/logout") == 0) {
		if (sess_tok[0]) {
			tcmg_log_dbg(D_WEBIF, "logout from %s", client_ip);
			session_invalidate(sess_tok);
		}
		send_redirect_clear_cookie(fd, "/login");
		req_free(&req);
		return;
	}

	/* ── Unauthenticated: redirect or 401 ── */
	if (!authed) {
		if (strcmp(p, "/login") == 0) {
			send_login_page(fd, 0);
		} else if (strncmp(p, "/api/", 5) == 0 || strcmp(p, "/logpoll") == 0) {
			send_json_401(fd);
		} else {
			send_redirect(fd, "/login");
		}
		req_free(&req);
		return;
	}

	/* ── GET /login while already authed → redirect ── */
	if (strcmp(p, "/login") == 0)
		{ send_redirect(fd, "/status"); req_free(&req); return; }

	/* ── Authenticated routing ── */
	if (strcmp(p, "/") == 0)
		send_redirect(fd, "/status");

	else if (strcmp(p, "/status") == 0) {
		char killstr[16] = "", kill_user[CFGKEY_LEN] = "";
		get_param(qs, "kill", killstr, sizeof(killstr));
		if (killstr[0]) {
			uint32_t tid = (uint32_t)strtoul(killstr, NULL, 10);
			get_param(qs, "user", kill_user, sizeof(kill_user));
			client_kill_by_tid(tid);
			tcmg_log("disconnect user '%s' tid=%u (by webif)",
			         kill_user[0] ? kill_user : "?", tid);
		}
		send_page_status(fd);
	}

	else if (strcmp(p, "/users")   == 0) send_page_users(fd);
	else if (strcmp(p, "/failban") == 0) send_page_failban(fd, qs);
	else if (strcmp(p, "/config")  == 0) send_page_config(fd);
	else if (strcmp(p, "/livelog") == 0) send_page_livelog(fd);
	else if (strcmp(p, "/logpoll") == 0) send_logpoll(fd, qs);
	else if (strcmp(p, "/power")   == 0) send_page_power(fd, qs);
	else if (strcmp(p, "/restart") == 0) send_page_restart(fd, qs);
	else if (strcmp(p, "/shutdown")== 0) send_page_shutdown(fd, qs);
	else if (strcmp(p, "/tvcas")   == 0) send_page_tvcas(fd);

	else if (strcmp(p, "/config_save") == 0 && strcmp(req.method, "POST") == 0)
		handle_config_save(fd, req.body ? req.body : "");

	else if (strcmp(p, "/srvid2_save") == 0 && strcmp(req.method, "POST") == 0)
		handle_srvid2_save(fd, req.body ? req.body : "");

	/* JSON API */
	else if (strcmp(p, "/api/status")            == 0) send_api_status(fd);
	else if (strcmp(p, "/api/user/toggle")       == 0) handle_user_toggle(fd, qs);
	else if (strcmp(p, "/api/user/get")          == 0) send_api_user_get(fd, qs);
	else if (strcmp(p, "/api/user/resetstats")   == 0) handle_user_resetstats(fd, qs);
	else if (strcmp(p, "/api/user/delete")       == 0) handle_user_delete(fd, qs);
	else if (strcmp(p, "/api/user/save") == 0 && strcmp(req.method, "POST") == 0)
		handle_user_save(fd, req.body ? req.body : "");
	else if (strcmp(p, "/api/user/add")  == 0 && strcmp(req.method, "POST") == 0)
		handle_user_add(fd, req.body ? req.body : "");

	else if (strcmp(p, "/api/reload") == 0) {
		g_reload_cfg = 1;
		send_json(fd, "{\"ok\":true,\"msg\":\"reload scheduled\"}");
	}
	else if (strcmp(p, "/api/restart") == 0) {
		tcmg_log("restart requested via API");
		g_restart = 1; g_running = 0;
		send_json(fd, "{\"ok\":true,\"msg\":\"restart initiated\"}");
	}
	else if (strcmp(p, "/api/resetstats") == 0) {
		handle_reset_stats();
		send_json(fd, "{\"ok\":true,\"msg\":\"stats reset\"}");
	}
	else {
		static const char not_found[] =
			"<html><body style='background:#090d14;color:#e8f0fe;"
			"font-family:monospace;display:flex;align-items:center;"
			"justify-content:center;height:100vh'>"
			"<div><h1 style='color:#3b82f6'>404</h1><p>Not Found</p>"
			"<a href='/status' style='color:#60a5fa'>&#8592; Back to Status</a>"
			"</div></body></html>";
		send_response(fd, 404, "Not Found", "text/html",
		              not_found, (int)strlen(not_found));
	}

	req_free(&req);
}

static void *http_server_thread(void *arg)
{
	(void)arg;
	tcmg_log("listening http %s:%d",
	         g_cfg.webif_bindaddr[0] ? g_cfg.webif_bindaddr : "0.0.0.0",
	         g_cfg.webif_port);

	while (s_webif_running) {
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(s_webif_sock, &rfds);
		struct timeval tv = { 1, 0 };
		if (select(s_webif_sock + 1, &rfds, NULL, NULL, &tv) <= 0)
			continue;

		struct sockaddr_in ca;
		socklen_t clen = sizeof(ca);
		int cfd = accept(s_webif_sock, (struct sockaddr *)&ca, &clen);
		if (cfd < 0) {
			if (s_webif_running)
				tcmg_log_dbg(D_WEBIF, "accept() errno=%d", errno);
			continue;
		}

		char client_ip[MAXIPLEN];
		inet_ntop(AF_INET, &ca.sin_addr, client_ip, sizeof(client_ip));

		/* Disable Nagle so small JSON responses flush immediately */
		int nodelay = 1;
		setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, SO_CAST(&nodelay), sizeof(nodelay));

		tcmg_log_dbg(D_WEBIF, "HTTP connection from %s", client_ip);

		s_conn_arg *ca2 = (s_conn_arg *)malloc(sizeof(s_conn_arg));
		if (ca2) {
			ca2->fd = cfd;
			tcmg_strlcpy(ca2->ip, client_ip, MAXIPLEN);
			pthread_t       t;
			pthread_attr_t  a;
			pthread_attr_init(&a);
			pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
			pthread_attr_setstacksize(&a, 128 * 1024);
			if (pthread_create(&t, &a, conn_thread, ca2) == 0) {
				pthread_attr_destroy(&a);
				continue;
			}
			pthread_attr_destroy(&a);
			free(ca2);
		}
		handle_request(cfd, client_ip);
		close(cfd);
	}

	tcmg_log("stopped");
	return NULL;
}

int32_t webif_start(void)
{
	if (!g_cfg.webif_enabled) { tcmg_log("disabled in config"); return -1; }

	s_webif_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (s_webif_sock < 0) {
		tcmg_log("socket() failed: %s", strerror(errno));
		return -1;
	}

	int opt = 1;
	setsockopt(s_webif_sock, SOL_SOCKET, SO_REUSEADDR, SO_CAST(&opt), sizeof(opt));
#ifdef SO_REUSEPORT
	setsockopt(s_webif_sock, SOL_SOCKET, SO_REUSEPORT, SO_CAST(&opt), sizeof(opt));
#endif

	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port   = htons((uint16_t)g_cfg.webif_port);
	if (g_cfg.webif_bindaddr[0])
		inet_pton(AF_INET, g_cfg.webif_bindaddr, &sa.sin_addr);
	else
		sa.sin_addr.s_addr = INADDR_ANY;

	if (bind(s_webif_sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		tcmg_log("bind() port %d failed: %s", g_cfg.webif_port, strerror(errno));
		close(s_webif_sock); s_webif_sock = -1; return -1;
	}
	if (listen(s_webif_sock, 128) < 0) {
		tcmg_log("listen() failed: %s", strerror(errno));
		close(s_webif_sock); s_webif_sock = -1; return -1;
	}

	s_webif_running = 1;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	pthread_attr_setstacksize(&attr, 256 * 1024);
	if (pthread_create(&s_webif_tid, &attr, http_server_thread, NULL) != 0) {
		tcmg_log("pthread_create failed");
		s_webif_running = 0;
		close(s_webif_sock); s_webif_sock = -1;
		pthread_attr_destroy(&attr);
		return -1;
	}
	pthread_attr_destroy(&attr);
	return 0;
}

void webif_stop(void)
{
	if (!s_webif_running) return;
	s_webif_running = 0;
	if (s_webif_sock >= 0) { close(s_webif_sock); s_webif_sock = -1; }
	pthread_join(s_webif_tid, NULL);
}
