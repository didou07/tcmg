#define MODULE_LOG_PREFIX "webif"
#include "tcmg-globals.h"
#include "tcmg-crypto.h"
#include "tcmg-log.h"

#ifndef TCMG_OS_WINDOWS
#  include <netdb.h>
#  include <sys/select.h>
#endif

#include "tcmg-webif-internal.h"

/* Private state for server thread */
static pthread_t s_webif_tid_priv;

static void *http_server_thread(void *arg)
{
	(void)arg;
	tcmg_log("listening http %s:%d",
	         g_cfg.webif_bindaddr[0] ? g_cfg.webif_bindaddr : "0.0.0.0",
	         g_cfg.webif_port);

	while (s_webif_running)
	{
		struct sockaddr_in ca;
		socklen_t clen = sizeof(ca);
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(s_webif_sock, &rfds);
		struct timeval tv = { 1, 0 };
		int rc = select(s_webif_sock + 1, &rfds, NULL, NULL, &tv);
		if (rc <= 0) continue;

		int cfd = accept(s_webif_sock, (struct sockaddr *)&ca, &clen);
		if (cfd < 0) { if (s_webif_running) tcmg_log_dbg(D_WEBIF, "accept() errno=%d", errno); continue; }

		char client_ip[MAXIPLEN];
		inet_ntop(AF_INET, &ca.sin_addr, client_ip, sizeof(client_ip));

		char peek[128] = "";
		recv(cfd, RECV_CAST(peek), sizeof(peek)-1, MSG_PEEK);
		int is_poll = (strstr(peek, "GET /logpoll") != NULL);
		if (!is_poll) tcmg_log_dbg(D_WEBIF, "HTTP connection from %s", client_ip);

		handle_request(cfd, client_ip);
		close(cfd);
	}
	tcmg_log("stopped");
	return NULL;
}

int32_t webif_start(void)
{
	int opt;
	struct sockaddr_in sa;
	if (!g_cfg.webif_enabled) { tcmg_log("disabled in config"); return -1; }

	s_webif_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (s_webif_sock < 0) { tcmg_log("socket() failed: %s", strerror(errno)); return -1; }

	opt = 1;
	setsockopt(s_webif_sock, SOL_SOCKET, SO_REUSEADDR, SO_CAST(&opt), sizeof(opt));

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
	if (listen(s_webif_sock, 16) < 0) {
		tcmg_log("listen() failed: %s", strerror(errno));
		close(s_webif_sock); s_webif_sock = -1; return -1;
	}

	s_webif_running = 1;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	pthread_attr_setstacksize(&attr, 256 * 1024);

	if (pthread_create(&s_webif_tid_priv, &attr, http_server_thread, NULL) != 0) {
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
	pthread_join(s_webif_tid_priv, NULL);
}
