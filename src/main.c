#define MODULE_LOG_PREFIX "main"
#include "tcmg.h"
#include "client.h"

static char **g_argv_saved = NULL;

static void print_usage(const char *prog)
{
	printf("\nUsage: %s [options]\n\n"
	       "Options:\n"
	       "  -c <dir>    Config directory (default: %s)\n"
	       "              Loads <dir>/" TCMG_CFG_FILE "\n"
	       "  -b          Run in background (daemonize)\n"
	       "  -d <level>  Debug bitmask (decimal)\n"
	       "                1=net   2=client  4=ecm    8=proto\n"
	       "               16=conf 32=webif  64=ban   65535=all\n"
	       "  -v          Show version and exit\n"
	       "  -h          Show this help\n\n",
	       prog, CS_CONFDIR);
}

int main(int argc, char *argv[])
{
	int            srv_fd, rc;
	struct sockaddr_in sa;
	pthread_attr_t attr;
	char           srvidpath[CFGPATH_LEN];
	char           cfgpath[CFGPATH_LEN];

	g_start_time = time(NULL);
	g_argv_saved = argv;
	tcmg_winsock_init();
	tcmg_setup_signals(&g_running);

	int do_daemon = 0;
	(void)do_daemon;

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-c") == 0 && i + 1 < argc)
		{
			tcmg_strlcpy(g_cfgdir, argv[++i], CFGPATH_LEN);
		}
		else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc)
		{
			long v = strtol(argv[++i], NULL, 0);
			if (v >= 0 && v <= 0xFFFF)
				g_dblevel = (uint16_t)v;
		}
		else if (strcmp(argv[i], "-b") == 0)
		{
			do_daemon = 1;
		}
		else if (strcmp(argv[i], "-v") == 0)
		{
			printf("%s\n", TCMG_BANNER);
			return 0;
		}
		else if (strcmp(argv[i], "-h") == 0)
		{
			print_usage(argv[0]);
			return 0;
		}
	}

	if (do_daemon && tcmg_daemonise() < 0)
		return 1;

	printf(
		"\n"
		"  _____  ____  __  __  ____\n"
		" |_   _|/ ___||  \\/  |/ ___|\n"
		"   | |  | |    | |\\/| | |  _\n"
		"   | |  | |___ | |  | | |_| |\n"
		"   |_|   \\____||_|  |_|\\____|\n"
		"\n"
		"  v" TCMG_VERSION "  --  built " TCMG_BUILD_TIME "\n"
		"\n");

	memset(&g_cfg, 0, sizeof(g_cfg));
	pthread_rwlock_init(&g_cfg.acc_lock, NULL);
	pthread_mutex_init(&g_cfg.ban_lock, NULL);

	tcmg_build_path(cfgpath, sizeof(cfgpath), g_cfgdir, TCMG_CFG_FILE);
	tcmg_mkdir(g_cfgdir);

	if (!cfg_load(cfgpath, &g_cfg))
	{
		tcmg_log("config not found: %s -- writing defaults", cfgpath);
		cfg_write_default(cfgpath);
		if (!cfg_load(cfgpath, &g_cfg))
		{
			tcmg_log("fatal: cannot load generated config %s", cfgpath);
			return 1;
		}
	}

	tcmg_log_dbg(D_CONF, "config loaded: %s", g_cfg.config_file);
	cfg_print(&g_cfg);
	log_ecm_set(g_cfg.ecm_log);

	tcmg_build_path(srvidpath, sizeof(srvidpath), g_cfgdir, TCMG_SRVID_FILE);
	{
		FILE *chk = fopen(srvidpath, "r");
		if (!chk)
		{
			tcmg_log("srvid: %s not found -- writing defaults", srvidpath);
			if (!srvid_write_default(srvidpath))
				tcmg_log("srvid: cannot create %s", srvidpath);
		}
		else fclose(chk);
	}
	{
		int n = srvid_load(srvidpath);
		if (n >= 0)
			tcmg_log("srvid: loaded %d channel(s) from %s", n, srvidpath);
	}

	if (g_cfg.logfile[0])
	{
		log_set_file(g_cfg.logfile);
		tcmg_log("logging to file: %s", g_cfg.logfile);
	}
	if (g_cfg.usrfile[0])
	{
		log_set_usrfile(g_cfg.usrfile);
		tcmg_log("user statistics file: %s", g_cfg.usrfile);
	}

	emu_init();
	webif_start();

	srv_fd = (int)socket(AF_INET, SOCK_STREAM, 0);
	if (srv_fd < 0) { perror("socket"); return 1; }

	{ int opt = 1;
	  setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, SO_CAST(&opt), sizeof(opt)); }

	memset(&sa, 0, sizeof(sa));
	sa.sin_family      = AF_INET;
	sa.sin_addr.s_addr = INADDR_ANY;
	sa.sin_port        = htons((uint16_t)g_cfg.port);

	if (bind(srv_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
		{ perror("bind"); close(srv_fd); return 1; }
	if (listen(srv_fd, 128) < 0)
		{ perror("listen"); close(srv_fd); return 1; }

	tcmg_log("newcamd_mgcamd listening on port %d", g_cfg.port);

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setstacksize(&attr, 256 * 1024);

	while (g_running)
	{
		if (g_reload_cfg)
		{
			g_reload_cfg = 0;
			char errbuf[256] = "";
			if (cfg_reload(g_cfg.config_file, errbuf, sizeof(errbuf)))
			{
				tcmg_log("config reloaded OK (%d accounts)", g_cfg.naccounts);
				srvid_load(srvidpath);
				clients_relink_accounts();
			}
			else
				tcmg_log("config reload FAILED: %s", errbuf);
		}

		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(srv_fd, &rfds);
		struct timeval tv = { 1, 0 };
		if (select(srv_fd + 1, &rfds, NULL, NULL, &tv) <= 0)
			continue;

		struct sockaddr_in ca;
		socklen_t clen = sizeof(ca);
		int cfd = (int)accept(srv_fd, (struct sockaddr *)&ca, &clen);
		if (cfd < 0)
		{
			if (g_running)
				tcmg_log_dbg(D_NET, "accept() errno=%d", errno);
			continue;
		}

		if (__sync_fetch_and_add(&g_active_conns, 1) >= MAX_CONNS)
		{
			__sync_fetch_and_sub(&g_active_conns, 1);
			close(cfd);
			tcmg_log("MAX_CONNS=%d reached -- rejected", MAX_CONNS);
			continue;
		}

		S_CONN_ARGS *args = (S_CONN_ARGS *)malloc(sizeof(S_CONN_ARGS));
		if (!args)
		{
			__sync_fetch_and_sub(&g_active_conns, 1);
			close(cfd);
			tcmg_log("%s", "out of memory -- rejected connection");
			continue;
		}
		args->fd = cfd;
		inet_ntop(AF_INET, &ca.sin_addr, args->ip, MAXIPLEN);
		args->ip[MAXIPLEN - 1] = '\0';

		pthread_t tid;
		rc = pthread_create(&tid, &attr, handle_client, args);
		if (rc != 0)
		{
			tcmg_log("pthread_create failed: %d", rc);
			__sync_fetch_and_sub(&g_active_conns, 1);
			close(cfd);
			free(args);
		}
	}

	{
		int nc = g_active_conns;
		if (nc > 0)
			tcmg_log("shutting down -- waiting for %d client(s) to disconnect...", nc);
		else
			tcmg_log("%s", "shutting down");
	}

	webif_stop();
	pthread_attr_destroy(&attr);
	close(srv_fd);

	for (int w = 0; w < 50 && g_active_conns > 0; w++)
	{
		if (w == 0)
			tcmg_log("waiting for %d connection(s) to close...", g_active_conns);
		sleep(1);
	}
	if (g_active_conns > 0)
		tcmg_log("shutdown: %d connection(s) still open -- forcing exit",
		         g_active_conns);

	pthread_rwlock_wrlock(&g_cfg.acc_lock);
	cfg_accounts_free(&g_cfg);
	pthread_rwlock_unlock(&g_cfg.acc_lock);
	ban_free_all();
	srvid_free();
	pthread_rwlock_destroy(&g_cfg.acc_lock);
	pthread_mutex_destroy(&g_cfg.ban_lock);

	tcmg_winsock_cleanup();

	if (g_restart)
	{
		tcmg_log("%s", "restarting process...");
		tcmg_exec_restart(g_argv_saved);
	}

	return 0;
}
