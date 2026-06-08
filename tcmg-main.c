#define MODULE_LOG_PREFIX "main"
#include "globals.h"
#include "tcmg-client.h"

static char **g_argv_saved = NULL;

static void print_usage(const char *prog)
{
	printf("\nUsage: %s [options]\n\n"
	       "Options:\n"
	       "  -c <dir>    Config directory (default: %s)\n"
	       "              Loads <dir>/" TCMG_CFG_FILE "\n"
	       "  -b          Run in background (daemonize)\n"
	       "  -d <level>  Debug bitmask (hex or decimal)\n"
	       "                0x0001=wire    0x0002=ecm     0x0004=emu\n"
	       "                0x0008=newcamd 0x0010=cccam   0x0020=http\n"
	       "                0x0040=conn    0xFFFF=all\n"
	       "  -v          Show version and exit\n"
	       "  -h          Show this help\n\n",
	       prog, CS_CONFDIR);
}

int main(int argc, char *argv[])
{
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
		tcmg_log("config not found at %s -- writing defaults", cfgpath);
		cfg_write_default(cfgpath);
		if (!cfg_load(cfgpath, &g_cfg))
		{
			tcmg_log("FATAL cannot load generated config file=%s", cfgpath);
			return 1;
		}
	}

	tcmg_log("config loaded file=%s", g_cfg.config_file);
	cfg_print(&g_cfg);
	log_ecm_set(g_cfg.ecm_log);

	tcmg_build_path(srvidpath, sizeof(srvidpath), g_cfgdir, TCMG_SRVID_FILE);
	{
		FILE *chk = fopen(srvidpath, "r");
		if (!chk)
		{
			tcmg_log("srvid file not found at %s -- writing defaults", srvidpath);
			if (!srvid_write_default(srvidpath))
				tcmg_log("srvid FAILED to create default file=%s", srvidpath);
		}
		else fclose(chk);
	}
	{
		int n = srvid_load(srvidpath);
		if (n >= 0)
			tcmg_log("srvid loaded channels=%d file=%s", n, srvidpath);
	}

	if (g_cfg.logfile[0])
	{
		log_set_file(g_cfg.logfile);
		tcmg_log("log file=%s", g_cfg.logfile);
	}
	if (g_cfg.usrfile[0])
	{
		log_set_usrfile(g_cfg.usrfile);
		tcmg_log("user stats file=%s", g_cfg.usrfile);
	}

	log_init();
	emu_init();
	webif_start();
	cccam_start();
	newcamd_start();

	while (g_running)
	{
		if (g_reload_cfg)
		{
			g_reload_cfg = 0;
			char errbuf[256] = "";
			if (cfg_reload(g_cfg.config_file, errbuf, sizeof(errbuf)))
			{
				tcmg_log("reload: config OK accounts=%d", g_cfg.naccounts);
				srvid_load(srvidpath);
				clients_relink_accounts();
			}
			else
				tcmg_log("reload: config FAILED reason=%s", errbuf);
		}
		sleep(1);
	}

	{
		int nc = g_active_conns;
		if (nc > 0)
			tcmg_log("shutdown: waiting for %d active client(s) to disconnect", nc);
		else
			tcmg_log("%s", "shutdown: no active clients -- proceeding");
	}

	webif_stop();
	cccam_stop();
	newcamd_stop();

	for (int w = 0; w < 50 && g_active_conns > 0; w++)
	{
		if (w == 0)
			tcmg_log("shutdown: waiting for %d connection(s) to close", g_active_conns);
		sleep(1);
	}
	if (g_active_conns > 0)
		tcmg_log("shutdown: FORCED EXIT %d connection(s) still open --",
		         g_active_conns);

	pthread_rwlock_wrlock(&g_cfg.acc_lock);
	cfg_accounts_free(&g_cfg);
	pthread_rwlock_unlock(&g_cfg.acc_lock);
	ban_free_all();
	srvid_free();
	pthread_rwlock_destroy(&g_cfg.acc_lock);
	pthread_mutex_destroy(&g_cfg.ban_lock);

	tcmg_winsock_cleanup();

	log_flush();
	log_shutdown();

	if (g_restart)
	{
		tcmg_exec_restart(g_argv_saved);
	}

	return 0;
}
