#define MODULE_LOG_PREFIX "main"
#include "tcmg-globals.h"
#if !defined(_WIN32) && !defined(_WIN64) && !defined(__MINGW32__) && !defined(__MINGW64__)
#  include <unistd.h>
#endif

/* Globals */
S_CONFIG         g_cfg;
volatile int32_t g_running       = 1;
volatile int32_t g_reload_cfg    = 0;
volatile int32_t g_restart       = 0;

/* saved for restart */
static char **g_argv_saved = NULL;
volatile int32_t g_active_conns = 0;
time_t           g_start_time   = 0;
char             g_cfgdir[CFGPATH_LEN] = ".";

/* ECM CW cache */
S_CW_CACHE_ENTRY g_cw_cache[CW_CACHE_SIZE];
pthread_mutex_t  g_cw_cache_mtx = PTHREAD_MUTEX_INITIALIZER;

static bool cw_cache_lookup(const uint8_t *ecm_md5, uint8_t *cw_out)
{
	uint32_t idx = ((uint32_t)ecm_md5[0] | ((uint32_t)ecm_md5[1] << 8)) & (CW_CACHE_SIZE - 1);
	bool hit = false;
	pthread_mutex_lock(&g_cw_cache_mtx);
	S_CW_CACHE_ENTRY *e = &g_cw_cache[idx];
	if (e->valid && ct_memeq(e->ecm_md5, ecm_md5, 16) &&
	    (time(NULL) - e->ts) < CW_CACHE_TTL_S)
	{
		memcpy(cw_out, e->cw, CW_LEN);
		hit = true;
	}
	pthread_mutex_unlock(&g_cw_cache_mtx);
	return hit;
}

static void cw_cache_store(const uint8_t *ecm_md5, const uint8_t *cw)
{
	uint32_t idx = ((uint32_t)ecm_md5[0] | ((uint32_t)ecm_md5[1] << 8)) & (CW_CACHE_SIZE - 1);
	pthread_mutex_lock(&g_cw_cache_mtx);
	S_CW_CACHE_ENTRY *e = &g_cw_cache[idx];
	memcpy(e->ecm_md5, ecm_md5, 16);
	memcpy(e->cw,      cw,      CW_LEN);
	e->ts    = time(NULL);
	e->valid = 1;
	pthread_mutex_unlock(&g_cw_cache_mtx);
}

/* Active client registry */
S_CLIENT        *g_clients[MAX_ACTIVE_CLIENTS];
pthread_mutex_t  g_clients_mtx = PTHREAD_MUTEX_INITIALIZER;

static void client_register(S_CLIENT *cl)
{
	pthread_mutex_lock(&g_clients_mtx);
	for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++)
		if (!g_clients[i]) { g_clients[i] = cl; break; }
	pthread_mutex_unlock(&g_clients_mtx);
}

static void client_unregister(S_CLIENT *cl)
{
	pthread_mutex_lock(&g_clients_mtx);
	for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++)
		if (g_clients[i] == cl) { g_clients[i] = NULL; break; }
	pthread_mutex_unlock(&g_clients_mtx);
}

void client_kill_by_tid(uint32_t tid)
{
	pthread_mutex_lock(&g_clients_mtx);
	for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++)
		if (g_clients[i] && g_clients[i]->thread_id == tid)
			{ g_clients[i]->kill_flag = 1; break; }
	pthread_mutex_unlock(&g_clients_mtx);
}

/* After cfg_reload, re-point each connected client to the new S_ACCOUNT
 * object (looked up by username). Without this, cl->account is a dangling
 * pointer into freed memory — any field (sched_day_from, limits, ...) is
 * garbage and will produce false schedule denials or UAF crashes.       */
static void clients_relink_accounts(void)
{
	pthread_mutex_lock(&g_clients_mtx);
	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++)
	{
		S_CLIENT *cl = g_clients[i];
		if (!cl || !cl->user[0]) continue;
		S_ACCOUNT *a;
		for (a = g_cfg.accounts; a; a = a->next)
			if (strcmp(cl->user, a->user) == 0) break;
		cl->account = a;   /* NULL if account was deleted — kill the client */
		if (!a) cl->kill_flag = 1;
	}
	pthread_rwlock_unlock(&g_cfg.acc_lock);
	pthread_mutex_unlock(&g_clients_mtx);
}

/* Signal handling */
#ifndef TCMG_OS_WINDOWS

static void sig_handler(int sig)
{
	(void)sig;
	g_running = 0;
}

static void setup_signals(void)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_handler;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT,  &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	signal(SIGPIPE, SIG_IGN);
}

#else

static BOOL WINAPI console_handler(DWORD event)
{
	if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT ||
	    event == CTRL_CLOSE_EVENT)
	{
		g_running = 0;
		return TRUE;
	}
	return FALSE;
}

static void setup_signals(void)
{
	SetConsoleCtrlHandler(console_handler, TRUE);
}

#endif

/* Build cfgdir/filename path */
static void build_cfg_path(char *dst, size_t dstsz, const char *filename)
{
	strncpy(dst, g_cfgdir, dstsz - 1);
	dst[dstsz - 1] = '\0';
	size_t dlen = strlen(dst);
	if (dlen + 1 + strlen(filename) + 1 <= dstsz)
	{
		dst[dlen]     = '/';
		dst[dlen + 1] = '\0';
		strncat(dst, filename, dstsz - dlen - 2);
	}
#ifdef TCMG_OS_WINDOWS
	for (char *p = dst; *p; p++)
		if (*p == '/') *p = '\\';
#endif
}

static void print_usage(const char *prog)
{
	printf("\nUsage: %s [options]\n\n"
	       "Options:\n"
	       "  -c <dir>    Config directory (default: current dir)\n"
	       "              Loads <dir>/" TCMG_CFG_FILE "\n"
	       "  -d <level>  Debug bitmask (decimal or hex 0x...)\n"
	       "              Bits: 0001=trace 0002=net   0004=reader 0008=client\n"
	       "                    0010=ecm   0020=proto  0040=conf   0080=webif\n"
	       "                    0100=ban   FFFF=all\n"
	       "  -v          Show version and exit\n"
	       "  -h          Show this help\n\n",
	       prog);
}

/* Send login NAK */
static void nc_nak(S_CLIENT *cl, uint16_t sid, uint16_t mid, uint32_t pid)
{
	uint8_t r[3] = { MSG_CLIENT_LOGIN_NAK, 0, 0 };
	nc_send(cl, r, 3, sid, mid, pid);
}

/* Send ECM NAK (empty CW reply) */
static void ecm_send_nak(S_CLIENT *cl, uint8_t cmd,
                          uint16_t sid, uint16_t mid, uint32_t pid)
{
	uint8_t r[3] = { cmd, 0, 0 };
	nc_send(cl, r, 3, sid, mid, pid);
}

/* Check if current time is within account schedule */
static bool account_in_schedule(const S_ACCOUNT *acc)
{
	if (acc->sched_day_from < 0) return true;

	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);

	/* tm_wday: 0=Sun..6=Sat → Mon=0..Sun=6 */
	int wday = (tm.tm_wday == 0) ? 6 : (tm.tm_wday - 1);
	int hhmm = tm.tm_hour * 100 + tm.tm_min;

	bool day_ok;
	if (acc->sched_day_from <= acc->sched_day_to)
		day_ok = (wday >= acc->sched_day_from && wday <= acc->sched_day_to);
	else
		day_ok = (wday >= acc->sched_day_from || wday <= acc->sched_day_to);

	if (!day_ok) return false;

	if (acc->sched_hhmm_from <= acc->sched_hhmm_to)
		return (hhmm >= acc->sched_hhmm_from && hhmm < acc->sched_hhmm_to);
	else
		return (hhmm >= acc->sched_hhmm_from || hhmm < acc->sched_hhmm_to);
}

static bool handle_login(S_CLIENT *cl,
                          const uint8_t *data, int32_t dlen,
                          uint16_t sid, uint16_t mid, uint32_t pid)
{
	const char *ip = cl->ip;
	S_ACCOUNT  *acc;
	const char *user, *hash;
	size_t      umax, ulen;
	char        expected[64];
	uint8_t     spread[16];
	int         i;

	if (dlen < 4)
	{
		nc_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: short packet", ip);
		return false;
	}

	user = (const char *)(data + 3);
	umax = (size_t)(dlen - 3);
	ulen = strnlen(user, umax);
	if (ulen >= umax)
	{
		nc_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: malformed user field", ip);
		return false;
	}
	hash = user + ulen + 1;
	if ((hash - (const char *)data) >= dlen)
	{
		nc_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: malformed hash field", ip);
		return false;
	}

	if (ban_is_banned(ip))
	{
		nc_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: IP banned", ip);
		tcmg_log_dbg(D_BAN, "%s rejected (banned)", ip);
		return false;
	}

	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	acc = cfg_find_account(user);
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	if (!acc)
	{
		ban_record_fail(ip);
		nc_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: unknown user '%s'", ip, user);
		return false;
	}
	if (!acc->enabled)
	{
		nc_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: account disabled '%s'", ip, user);
		return false;
	}

	if (acc->nwhitelist > 0)
	{
		bool ok = false; int j;
		for (j = 0; j < acc->nwhitelist; j++)
			if (strncmp(acc->ip_whitelist[j], ip, MAXIPLEN) == 0)
			{ ok = true; break; }
		if (!ok)
		{
			nc_nak(cl, sid, mid, pid);
			tcmg_log("%s LOGIN failed: IP not whitelisted for '%s'", ip, user);
			return false;
		}
	}

	if (!crypt_md5_crypt(acc->pass, hash, expected, sizeof(expected)) ||
	    !ct_streq(expected, hash))
	{
		ban_record_fail(ip);
		nc_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: wrong password for '%s'", ip, user);
		tcmg_log_dbg(D_CLIENT, "%s bad password attempt for '%s'", ip, user);
		return false;
	}

	if (acc->expirationdate > 0 && time(NULL) > acc->expirationdate)
	{
		nc_nak(cl, sid, mid, pid);
		tcmg_log("LOGIN DENIED: account '%s' expired", acc->user);
		return false;
	}
	if (acc->max_connections > 0 && (int32_t)acc->active >= acc->max_connections)
	{
		nc_nak(cl, sid, mid, pid);
		tcmg_log("LOGIN DENIED: '%s' max_connections=%d reached", acc->user, acc->max_connections);
		return false;
	}

	{ uint8_t r[3] = { MSG_CLIENT_LOGIN_ACK, 0, 0 }; nc_send(cl, r, 3, sid, mid, pid); }

	{ size_t hlen = strlen(hash);
	  for (i = 0; i < (int)hlen; i++)
	      cl->session_key[i % 14] ^= (uint8_t)hash[i]; }
	crypt_key_spread(cl->session_key, spread);
	memcpy(cl->key1, spread,     8);
	memcpy(cl->key2, spread + 8, 8);
	secure_zero(spread, sizeof(spread));

	cl->caid      = acc->caid;
	cl->client_id = sid;
	cl->is_mgcamd = (acc->ncaids > 0) ? 1 : 0;
	strncpy(cl->user, acc->user, CFGKEY_LEN - 1);
	strncpy(cl->client_name, cfg_client_name(sid), sizeof(cl->client_name) - 1);
	cl->account   = acc;

	__sync_fetch_and_add(&acc->active, 1);
	acc->last_seen = time(NULL);
	if (acc->first_login == 0) acc->first_login = time(NULL);
	ban_record_ok(ip);

	if (cl->is_mgcamd)
	{
		char caids[64];
		int pos = snprintf(caids, sizeof(caids), "%04X", acc->caid);
		for (i = 0; i < acc->ncaids; i++)
			pos += snprintf(caids + pos, sizeof(caids) - pos, ",%04X", acc->caids[i]);
		tcmg_log("%s  %-12s  [%s]  %s", ip, user, caids, cl->client_name);
	}
	else
	{
		tcmg_log("%s  %-12s  %04X  %s", ip, user, acc->caid, cl->client_name);
	}
	tcmg_log_dbg(D_CLIENT, "%s authenticated '%s' caid=%04X", ip, user, acc->caid);
	return true;
}

static void handle_card(S_CLIENT *cl, uint16_t sid, uint16_t mid, uint32_t pid)
{
	uint8_t  resp[26];
	uint16_t caid = cl->account ? cl->account->caid : cl->caid;

	memset(resp, 0, sizeof(resp));
	resp[0] = MSG_CARD_DATA;
	resp[4] = (uint8_t)(caid >> 8);
	resp[5] = (uint8_t)(caid & 0xFF);
	nc_send(cl, resp, 26, sid, mid, pid);
	tcmg_log_dbg(D_ECM, "%s CARD_DATA CAID=%04X", cl->ip, caid);

	if (cl->is_mgcamd && cl->account)
	{
		int i;
		nc_send_addcard(cl, caid, 0, mid);
		for (i = 0; i < cl->account->ncaids; i++)
			if (cl->account->caids[i] != caid)
				nc_send_addcard(cl, cl->account->caids[i], 0, mid);
	}
}

static void handle_ecm(S_CLIENT *cl, uint8_t cmd,
                        const uint8_t *data, int32_t dlen,
                        uint16_t sid, uint16_t mid, uint32_t pid,
                        uint16_t caid_hdr)
{
	uint8_t   resp[32], cw[CW_LEN];
	int32_t   res;
	uint16_t  ecm_caid;
	S_ECM_CTX ctx;

	memset(cw, 0, CW_LEN);

	if (!cl->account)
	{
		ecm_send_nak(cl, cmd, sid, mid, pid);
		return;
	}

	if (!account_in_schedule(cl->account))
	{
		tcmg_log("%s ECM denied: outside schedule for '%s'", cl->ip, cl->user);
		ecm_send_nak(cl, cmd, sid, mid, pid);
		return;
	}

	ecm_caid = cl->caid;
	if (cl->is_mgcamd && caid_hdr)
	{
		bool ok = (cl->account->caid == caid_hdr);
		if (!ok)
		{
			int i;
			for (i = 0; i < cl->account->ncaids; i++)
				if (cl->account->caids[i] == caid_hdr) { ok = true; break; }
		}
		if (!ok)
		{
			tcmg_log("CAID %04X not permitted for user=%s", caid_hdr, cl->user);
			ecm_send_nak(cl, cmd, sid, mid, pid);
			return;
		}
		ecm_caid = caid_hdr;
	}

	if (cl->account->nsid_whitelist > 0)
	{
		bool sid_ok = false;
		int i;
		for (i = 0; i < cl->account->nsid_whitelist; i++)
			if (cl->account->sid_whitelist[i] == sid) { sid_ok = true; break; }
		if (!sid_ok)
		{
			tcmg_log_dbg(D_CLIENT, "%s SID %04X not in whitelist for '%s'",
			             cl->ip, sid, cl->user);
			ecm_send_nak(cl, cmd, sid, mid, pid);
			return;
		}
	}

	tcmg_log_dbg(D_CLIENT, "%s  ECM CAID=%04X SID=%04X len=%d", cl->ip, ecm_caid, sid, dlen);

	cl->last_ecm_time = time(NULL);
	cl->last_caid  = ecm_caid;
	cl->last_srvid = sid;
	/* Cache channel name for webif display */
	const char *ch = srvid_lookup(ecm_caid, sid);
	if (ch)
		strncpy(cl->last_channel, ch, sizeof(cl->last_channel) - 1);
	else
		cl->last_channel[0] = '\0';

	strncpy(ctx.user, cl->user, CFGKEY_LEN - 1);
	strncpy(ctx.ip,   cl->ip,   MAXIPLEN - 1);
	ctx.fd        = cl->fd;
	ctx.caid      = ecm_caid;
	ctx.thread_id = cl->thread_id;
	ctx.account   = cl->account;

	uint8_t ecm_md5[16];
	crypt_md5_hash(data, (size_t)dlen, ecm_md5);
	bool cache_hit = cw_cache_lookup(ecm_md5, cw);

	if (cache_hit)
	{
		tcmg_log_dbg(D_ECM, "%s ECM cache HIT CAID=%04X SID=%04X", cl->ip, ecm_caid, sid);
		res = EMU_OK;
	}
	else
	{
		res = emu_process(ecm_caid, sid, data, dlen, cw, &ctx);
		if (res == EMU_OK)
			cw_cache_store(ecm_md5, cw);
	}

	resp[0] = cmd;
	if (res == EMU_OK)
	{
		resp[1] = 0;
		resp[2] = CW_LEN;
		memcpy(resp + 3, cw, CW_LEN);
		nc_send(cl, resp, 19, sid, mid, pid);
		cl->account->last_seen = time(NULL);
	}
	else
	{
		resp[1] = resp[2] = 0;
		nc_send(cl, resp, 3, sid, mid, pid);
	}
	secure_zero(cw, sizeof(cw));
}

static void *handle_client(void *arg)
{
	S_CONN_ARGS *args = (S_CONN_ARGS *)arg;
	S_CLIENT     cl;
	uint8_t      data[NC_MSG_MAX];
	uint16_t     sid, mid, caid_hdr;
	uint32_t     pid;
	int32_t      dlen;

	memset(&cl, 0, sizeof(cl));
	cl.fd            = args->fd;
	cl.thread_id     = (uint32_t)(uintptr_t)pthread_self();
	cl.connect_time  = time(NULL);
	cl.last_ecm_time = time(NULL);
	strncpy(cl.ip, args->ip, MAXIPLEN - 1);
	free(args);

	client_register(&cl);
	tcmg_log("%s connected [%d active]", cl.ip, g_active_conns);
	tcmg_log_dbg(D_CLIENT, "%s new connection", cl.ip);

	nc_init(&cl, g_cfg.des_key, g_cfg.sock_timeout);

	while (g_running && !cl.kill_flag)
	{
		if (cl.account && cl.account->max_idle > 0)
		{
			time_t idle_secs = time(NULL) - cl.last_ecm_time;
			if (idle_secs >= cl.account->max_idle)
			{
				tcmg_log("%s idle timeout (%lds >= %ds) — disconnecting '%s'",
				         cl.ip, (long)idle_secs, cl.account->max_idle, cl.user);
				break;
			}
		}

		dlen = nc_recv(&cl, data, &sid, &mid, &pid, &caid_hdr);
		if (dlen < 0)
		{
			tcmg_log_dbg(D_CLIENT, "%s disconnect", cl.ip);
			tcmg_log("%s disconnected", cl.ip);
			break;
		}

		uint8_t cmd = data[0];
		tcmg_log_dbg(D_PROTO, "%s cmd=0x%02X sid=%04X len=%d", cl.ip, cmd, sid, dlen);

		if      (cmd == MSG_CLIENT_LOGIN)
		{ if (!handle_login(&cl, data, dlen, sid, mid, pid)) break; }
		else if (cmd == MSG_CARD_DATA_REQ) { handle_card(&cl, sid, mid, pid); }
		else if (cmd == MSG_KEEPALIVE)     { nc_send(&cl, data, dlen, sid, mid, pid); }
		else if (cmd == MSG_ECM_0 || cmd == MSG_ECM_1)
		{ handle_ecm(&cl, cmd, data, dlen, sid, mid, pid, caid_hdr); }
		else if (cmd == MSG_GET_VERSION)   { nc_send_version(&cl, mid); }
		else { tcmg_log_dbg(D_PROTO, "%s unknown cmd=0x%02X", cl.ip, cmd); }
	}

	client_unregister(&cl);
	if (cl.account)
		__sync_fetch_and_sub(&cl.account->active, 1);

	close(cl.fd);
	__sync_fetch_and_sub(&g_active_conns, 1);
	return NULL;
}

int main(int argc, char *argv[])
{
	int            srv_fd, rc;
	struct sockaddr_in sa;
	pthread_attr_t attr;
	char           srvidpath[CFGPATH_LEN];

	g_start_time = time(NULL);
	g_argv_saved = argv;
	tcmg_winsock_init();
	setup_signals();

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-c") == 0 && i + 1 < argc)
		{
			strncpy(g_cfgdir, argv[++i], CFGPATH_LEN - 1);
		}
		else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc)
		{
			long v = strtol(argv[++i], NULL, 0);
			if (v >= 0 && v <= 0xFFFF)
				g_dblevel = (uint16_t)v;
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

	char cfgpath[CFGPATH_LEN];
	build_cfg_path(cfgpath, sizeof(cfgpath), TCMG_CFG_FILE);

	if (!cfg_load(cfgpath, &g_cfg))
	{
		tcmg_log("config not found: %s — writing defaults", cfgpath);
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

	/* Load channel names */
	build_cfg_path(srvidpath, sizeof(srvidpath), TCMG_SRVID_FILE);

	/* If tcmg.srvid2 is missing from cfgdir, generate a default one */
	{
		FILE *chk = fopen(srvidpath, "r");
		if (!chk)
		{
			tcmg_log("srvid: %s not found — writing defaults", srvidpath);
			if (!srvid_write_default(srvidpath))
				tcmg_log("srvid: cannot create %s", srvidpath);
		}
		else
		{
			fclose(chk);
		}
	}

	int nsrv = srvid_load(srvidpath);
	if (nsrv >= 0)
		tcmg_log("srvid: loaded %d channel(s) from %s", nsrv, srvidpath);

	if (g_cfg.logfile[0])
	{
		log_set_file(g_cfg.logfile);
		tcmg_log("logging to file: %s", g_cfg.logfile);
	}

	emu_init();
	webif_start();

	srv_fd = (int)socket(AF_INET, SOCK_STREAM, 0);
	if (srv_fd < 0) { perror("socket"); return 1; }

	int opt = 1;
	setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

#ifdef TCMG_OS_MACOS
	setsockopt(srv_fd, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
#endif

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
		struct sockaddr_in ca;
		socklen_t  clen = sizeof(ca);
		int        cfd;
		pthread_t  tid;

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

		cfd = (int)accept(srv_fd, (struct sockaddr *)&ca, &clen);
		if (cfd < 0)
		{
			if (g_running)
				tcmg_log_dbg(D_NET, "accept() errno=%d", errno);
			continue;
		}

		if (__sync_fetch_and_add(&g_active_conns, 1) + 1 > MAX_CONNS)
		{
			__sync_fetch_and_sub(&g_active_conns, 1);
			close(cfd);
			tcmg_log("MAX_CONNS=%d reached — rejected", MAX_CONNS);
			continue;
		}

		S_CONN_ARGS *args = (S_CONN_ARGS *)malloc(sizeof(S_CONN_ARGS));
		if (!args)
		{
			__sync_fetch_and_sub(&g_active_conns, 1);
			close(cfd);
			continue;
		}
		args->fd = cfd;
		inet_ntop(AF_INET, &ca.sin_addr, args->ip, MAXIPLEN);

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
			tcmg_log("shutting down — waiting for %d client(s) to disconnect...", nc);
		else
			tcmg_log("shutting down");
	}
	webif_stop();
	pthread_attr_destroy(&attr);
	close(srv_fd);

	for (int _w = 0; _w < 50 && g_active_conns > 0; _w++)
	{
		if (_w == 0)
			tcmg_log("waiting for %d connection(s) to close...", g_active_conns);
#ifdef TCMG_OS_WINDOWS
		Sleep(100);
#else
		struct timespec ts = { 0, 100000000L };
		nanosleep(&ts, NULL);
#endif
	}
	if (g_active_conns > 0)
		tcmg_log("shutdown: %d connection(s) still open — forcing exit", g_active_conns);

	pthread_rwlock_wrlock(&g_cfg.acc_lock);
	cfg_accounts_free(&g_cfg);
	pthread_rwlock_unlock(&g_cfg.acc_lock);
	ban_free_all();
	srvid_free();
	pthread_rwlock_destroy(&g_cfg.acc_lock);
	pthread_mutex_destroy(&g_cfg.ban_lock);

	tcmg_winsock_cleanup();

	/* ── Restart if requested ── */
	if (g_restart)
	{
		tcmg_log("restarting process...");
#if defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__) || defined(__MINGW64__)
		/* Windows: spawn a new process and exit */
		STARTUPINFOA si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));
		char cmdline[4096] = "";
		for (int i = 0; g_argv_saved[i]; i++) {
			if (i > 0) strncat(cmdline, " ", sizeof(cmdline)-1);
			strncat(cmdline, g_argv_saved[i], sizeof(cmdline)-1);
		}
		if (CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
		}
#else
		/* Unix: re-exec same binary with same args — clean and portable */
		execv(g_argv_saved[0], g_argv_saved);
		/* execv only returns on error */
		perror("execv restart failed");
#endif
	}

	return 0;
}
