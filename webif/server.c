#define MODULE_LOG_PREFIX "webif"
#include "../src/core/utils.h"
#include "../src/core/runtime_state.h"
#include "../src/log/log.h"
#include "../src/client/client.h"
#include "../src/net/net.h"
#include "../src/security/failban.h"
#include "internal/proto.h"
#include "service/service.h"
#include <semaphore.h>
#include <stdatomic.h>

static _Atomic int8_t s_webif_running = 0;
static int             s_webif_sock    = -1;

static pthread_t s_webif_tid;
static sem_t     s_webif_sem;

#define WEBIF_MAX_THREADS 16

typedef struct { int fd; char ip[MAXIPLEN]; } s_conn_arg;

static void *conn_thread(void *arg)
{
	s_conn_arg *c = (s_conn_arg *)arg;
	log_set_type(LOG_TYPE_WEBIF);
	handle_request(c->fd, c->ip);
	close(c->fd);
	free(c);
	sem_post(&s_webif_sem);
	return NULL;
}

static int request_is_authed(const char *raw, const char *client_ip, char *sess_tok_out)
{
	sess_tok_out[0] = '\0';

	if (!webif_auth_enabled()) return 1;

	char cookie_hdr[512] = "";
	if (web_header_get(raw, "Cookie", cookie_hdr, sizeof(cookie_hdr))) {
		char tok[WEB_SESSION_LEN + 1];
		const char *sess = cookie_get_session(cookie_hdr, tok, sizeof(tok));
		if (sess && session_check(sess)) {
			tcmg_strlcpy(sess_tok_out, sess, WEB_SESSION_LEN + 1);
			return 1;
		}
	}

	char auth_hdr[512] = "";
	if (web_header_get(raw, "Authorization", auth_hdr, sizeof(auth_hdr))) {
		                                                                        
                                                                       
		if (ban_is_banned(client_ip)) return 0;
		if (check_auth(auth_hdr)) return 1;
		ban_record_fail(client_ip);
		tcmg_log("webif BASIC auth failed: from=%s", client_ip);
	}

	return 0;
}

                                                                             
                                                                            
                                                                                
                                                                             
static int is_state_changing(const char *method, const char *path, const char *qs)
{
	if (strcmp(method, "POST") == 0) return 1;
	if (strncmp(path, "/api/user/", 10) == 0 && strcmp(path, "/api/user/get") != 0) return 1;
	if (strncmp(path, "/api/failban/", 13) == 0) return 1;
	if (strcmp(path, "/api/reload")     == 0 || strcmp(path, "/api/restart") == 0 ||
	    strcmp(path, "/api/resetstats") == 0 || strcmp(path, "/restart")     == 0 ||
	    strcmp(path, "/shutdown")       == 0) return 1;
	if (strcmp(path, "/power")   == 0 && strstr(qs, "confirm="))  return 1;
	if (strcmp(path, "/failban") == 0 && strstr(qs, "action="))   return 1;
	if (strcmp(path, "/status")  == 0 && strstr(qs, "kill="))     return 1;
	if (strcmp(path, "/logpoll") == 0 && strstr(qs, "debug="))    return 1;
	return 0;
}

static int csrf_blocked(const char *raw)
{
	char v[128] = "";
	if (web_header_get(raw, "Sec-Fetch-Site", v, sizeof(v)) && strcmp(v, "cross-site") == 0)
		return 1;
	char origin[256] = "", host[256] = "";
	if (web_header_get(raw, "Origin", origin, sizeof(origin)) && strcmp(origin, "null") != 0 &&
	    web_header_get(raw, "Host", host, sizeof(host))) {
		const char *o = strstr(origin, "://");
		o = o ? o + 3 : origin;
		if (strcmp(o, host) != 0) return 1;
	}
	return 0;
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

	if (!strstr(raw, "\r\n\r\n") && rlen >= WEB_BUF_SIZE - 1) {
		free(raw);
		send_json_error(fd, 431, "Request Header Fields Too Large", "headers too large");
		return;
	}

	s_http_req req;
	if (!req_parse(&req, fd, raw, rlen)) { free(raw); return; }
	if (req.status) {
		int st = req.status;
		free(raw);
		req_free(&req);
		send_json_error(fd, st,
			st == 413 ? "Payload Too Large" : st == 414 ? "URI Too Long" :
			st == 503 ? "Service Unavailable" : "Bad Request",
			st == 413 ? "request body too large" : st == 414 ? "uri too long" :
			st == 503 ? "out of memory" : "bad request");
		return;
	}

	if (strcmp(req.path, "/logpoll") != 0)
		tcmg_log_dbg(D_HTTP, "HTTP %s %s%s%s", req.method, req.path,
		             req.qs[0] ? "?" : "", req.qs);

	char sess_tok[WEB_SESSION_LEN + 1];
	int  authed = request_is_authed(raw, client_ip, sess_tok);
	int  csrf   = authed && is_state_changing(req.method, req.path, req.qs) && csrf_blocked(raw);
	free(raw); raw = NULL;

	const char *p  = req.path;
	const char *qs = req.qs;

	if (strcmp(p, "/login") == 0 && strcmp(req.method, "POST") == 0) {
		char u[CFGKEY_LEN] = {0}, pw[CFGKEY_LEN] = {0};
		form_get(req.body, "u",  u,  sizeof(u));
		form_get(req.body, "p", pw, sizeof(pw));
		if (ban_is_banned(client_ip)) {
			send_login_page(fd, 2);
			req_free(&req);
			return;
		}
		if (check_credentials(u, pw)) {
			ban_record_ok(client_ip);
			char token[WEB_SESSION_LEN + 1];
			session_create(token);
			tcmg_log_dbg(D_HTTP, "webif LOGIN ok user='%s' from=%s", u, client_ip);
			send_redirect_with_cookie(fd, "/status", token);
		} else {
			ban_record_fail(client_ip);
			tcmg_log("webif LOGIN failed: user='%s' from=%s", u, client_ip);
			send_login_page(fd, 1);
		}
		req_free(&req);
		return;
	}

	if (strcmp(p, "/logout") == 0) {
		if (sess_tok[0]) {
			tcmg_log_dbg(D_HTTP, "logout from=%s", client_ip);
			session_invalidate(sess_tok);
		}
		send_redirect_clear_cookie(fd, "/login");
		req_free(&req);
		return;
	}

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

	if (strcmp(p, "/login") == 0)
		{ send_redirect(fd, "/status"); req_free(&req); return; }

	if (csrf) {
		tcmg_log("webif: cross-site request blocked: %s %s from=%s", req.method, p, client_ip);
		send_json_error(fd, 403, "Forbidden", "cross-site request blocked");
		req_free(&req);
		return;
	}

	if (strcmp(p, "/") == 0)
		send_redirect(fd, "/status");

	else if (strcmp(p, "/status") == 0) {
		char killstr[16] = "", kill_user[CFGKEY_LEN] = "";
		get_param(qs, "kill", killstr, sizeof(killstr));
		if (killstr[0]) {
			char *end = NULL;
			errno = 0;
			unsigned long tid_u = strtoul(killstr, &end, 10);
			if (errno == 0 && end && *end == '\0' && tid_u <= UINT32_MAX) {
				uint32_t tid = (uint32_t)tid_u;
				get_param(qs, "user", kill_user, sizeof(kill_user));
				webif_client_kill_by_tid(tid);
				tcmg_log("webif: disconnect user='%s' tid=%u (requested via webif)",
				         kill_user[0] ? kill_user : "?", tid);
			}
		}
		send_page_status(fd);
	}

	else if (strcmp(p, "/users")   == 0) send_page_users(fd);
	else if (strcmp(p, "/readers") == 0) send_page_readers(fd);
	else if (strcmp(p, "/failban") == 0) send_page_failban(fd, qs);
	else if (strcmp(p, "/config")  == 0) send_page_config(fd);
	else if (strcmp(p, "/files")   == 0) send_page_files(fd);
	else if (strcmp(p, "/livelog") == 0) send_page_livelog(fd);
	else if (strcmp(p, "/logpoll") == 0) send_logpoll(fd, qs);
	else if (strcmp(p, "/power")   == 0) send_page_power(fd, qs);
	else if (strcmp(p, "/restart") == 0) send_page_restart(fd, qs);
	else if (strcmp(p, "/shutdown")== 0) send_page_shutdown(fd, qs);
	else if (strcmp(p, "/tvcas")   == 0) send_page_tvcas(fd);

	else if (strcmp(p, "/api/status")               == 0) send_api_status(fd, qs);
	else if (strcmp(p, "/api/pcsc/readers")         == 0) send_api_pcsc_readers(fd);
	else if (strcmp(p, "/api/readers")             == 0) send_api_readers(fd);
	else if (strcmp(p, "/api/reader/get")           == 0) send_api_reader_get(fd, qs);
	else if (strcmp(p, "/api/reader/save") == 0 && strcmp(req.method, "POST") == 0)
		handle_api_reader_save(fd, req.body ? req.body : "");
	else if (strcmp(p, "/api/reader/delete")        == 0) handle_api_reader_delete(fd, qs);
	else if (strcmp(p, "/api/userstats")              == 0) send_api_userstats(fd);
	else if (strcmp(p, "/api/user/toggle")           == 0) handle_user_toggle(fd, qs);
	else if (strcmp(p, "/api/user/get")              == 0) send_api_user_get(fd, qs);
	else if (strcmp(p, "/api/user/resetstats")       == 0) handle_user_resetstats(fd, qs);
	else if (strcmp(p, "/api/user/delete")           == 0) handle_user_delete(fd, qs);
	else if (strcmp(p, "/api/user/save")  == 0 && strcmp(req.method, "POST") == 0)
		handle_user_save(fd, req.body ? req.body : "");
	else if (strcmp(p, "/api/user/add")   == 0 && strcmp(req.method, "POST") == 0)
		handle_user_add(fd, req.body ? req.body : "");

	else if (strcmp(p, "/api/config/get")  == 0) send_api_config_get(fd);
	else if (strcmp(p, "/api/config/save") == 0 && strcmp(req.method, "POST") == 0)
		handle_api_config_save(fd, req.body ? req.body : "");
	else if (strcmp(p, "/api/config/file/get") == 0) send_api_file_get(fd, qs);
	else if (strcmp(p, "/api/config/file/save") == 0 && strcmp(req.method, "POST") == 0)
		handle_api_file_save(fd, req.body ? req.body : "");

	else if (strcmp(p, "/api/failban/clear")    == 0) handle_api_failban_clear(fd, qs);
	else if (strcmp(p, "/api/failban/clearall") == 0) handle_api_failban_clearall(fd);

	else if (strcmp(p, "/api/reload")     == 0) handle_api_reload(fd);
	else if (strcmp(p, "/api/restart")    == 0) handle_api_restart(fd);
	else if (strcmp(p, "/api/resetstats") == 0) handle_api_resetstats(fd);

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
	log_set_type(LOG_TYPE_WEBIF);
	char bindaddr[MAXIPLEN];
	webif_bindaddr(bindaddr, sizeof(bindaddr));
	tcmg_log("listening http %s:%d", bindaddr[0] ? bindaddr : "0.0.0.0", webif_port());

	while (atomic_load_explicit(&s_webif_running, memory_order_acquire)) {
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(s_webif_sock, &rfds);
		struct timeval tv = { 0, 200000 };
		if (select(s_webif_sock + 1, &rfds, NULL, NULL, &tv) <= 0)
			continue;

		struct sockaddr_in ca;
		socklen_t clen = sizeof(ca);
		int cfd = accept(s_webif_sock, (struct sockaddr *)&ca, &clen);
		if (cfd < 0) {
			if (atomic_load_explicit(&s_webif_running, memory_order_acquire))
				tcmg_log_dbg(D_HTTP, "accept() failed errno=%d (%s)", errno, strerror(errno));
			continue;
		}

		char client_ip[MAXIPLEN];
		inet_ntop(AF_INET, &ca.sin_addr, client_ip, sizeof(client_ip));

		int nodelay = 1;
		setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, SO_CAST(&nodelay), sizeof(nodelay));

		tcmg_log_dbg(D_HTTP, "webif HTTP connection from=%s fd=%d", client_ip, cfd);

		s_conn_arg *ca2 = (s_conn_arg *)malloc(sizeof(s_conn_arg));
		if (ca2 && sem_trywait(&s_webif_sem) == 0) {
			pthread_t       t;
			pthread_attr_t  a;
			ca2->fd = cfd;
			tcmg_strlcpy(ca2->ip, client_ip, MAXIPLEN);
			pthread_attr_init(&a);
			pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
			pthread_attr_setstacksize(&a, 128 * 1024);
			if (pthread_create(&t, &a, conn_thread, ca2) == 0) {
				pthread_attr_destroy(&a);
				continue;
			}
			pthread_attr_destroy(&a);
			sem_post(&s_webif_sem);
			free(ca2);
		} else {
			free(ca2);
			static const char busy[] = "<html><body>WebIF busy</body></html>";
			send_response(cfd, 503, "Service Unavailable", "text/html", busy, (int)strlen(busy));
		}
		close(cfd);
	}

	tcmg_log("%s", "stopped");
	return NULL;
}

int32_t webif_start(void)
{
	S_WEBIF_CONFIG_VIEW cfg;
	if (!webif_config_snapshot(&cfg) || !cfg.webif_enabled) { tcmg_log_dbg(D_HTTP, "%s", "disabled in config"); return -1; }

	s_webif_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (s_webif_sock < 0) {
		tcmg_log("socket() failed: errno=%d (%s)", errno, strerror(errno));
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
	sa.sin_port   = htons((uint16_t)cfg.webif_port);
	if (cfg.webif_bindaddr[0]) {
		if (inet_pton(AF_INET, cfg.webif_bindaddr, &sa.sin_addr) != 1) {
			tcmg_log("invalid webif BINDADDR '%s' -- refusing to listen on all interfaces",
			         cfg.webif_bindaddr);
			close(s_webif_sock); s_webif_sock = -1; return -1;
		}
	} else
		sa.sin_addr.s_addr = INADDR_ANY;

	if (bind(s_webif_sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		tcmg_log("bind() failed: port=%d errno=%d (%s)", cfg.webif_port, errno, strerror(errno));
		close(s_webif_sock); s_webif_sock = -1; return -1;
	}
	if (listen(s_webif_sock, 128) < 0) {
		tcmg_log("listen() failed: errno=%d (%s)", errno, strerror(errno));
		close(s_webif_sock); s_webif_sock = -1; return -1;
	}

	sem_init(&s_webif_sem, 0, WEBIF_MAX_THREADS);
	atomic_store_explicit(&s_webif_running, 1, memory_order_release);
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	pthread_attr_setstacksize(&attr, 256 * 1024);
	if (pthread_create(&s_webif_tid, &attr, http_server_thread, NULL) != 0) {
		tcmg_log("pthread_create failed: errno=%d (%s)", errno, strerror(errno));
		atomic_store_explicit(&s_webif_running, 0, memory_order_release);
		sem_destroy(&s_webif_sem);
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
	atomic_store_explicit(&s_webif_running, 0, memory_order_release);
	pthread_join(s_webif_tid, NULL);                                                     
	if (s_webif_sock >= 0) { close(s_webif_sock); s_webif_sock = -1; }
	sem_destroy(&s_webif_sem);
}
