#define MODULE_LOG_PREFIX "newcamd"
#include "../../globals.h"
#include "newcamd.h"
#include "../client/client.h"

static _Atomic int32_t   s_ncd_running = 0;
static pthread_t         s_ncd_thread;
static int               s_ncd_srv_fd  = -1;

static void ncd_nak(S_CLIENT *cl, uint16_t sid, uint16_t mid, uint32_t pid)
{
	uint8_t r[3] = { MSG_CLIENT_LOGIN_NAK, 0, 0 };
	nc_send(cl, r, 3, sid, mid, pid);
}

static void ncd_ecm_nak(S_CLIENT *cl, uint8_t cmd,
                         uint16_t sid, uint16_t mid, uint32_t pid)
{
	uint8_t r[3] = { cmd, 0, 0 };
	nc_send(cl, r, 3, sid, mid, pid);
}

static bool account_in_schedule(const S_ACCOUNT *acc)
{
	if (acc->sched_day_from < 0) return true;

	time_t    now = time(NULL);
	struct tm tm_buf;
	localtime_r(&now, &tm_buf);

	int wday = (tm_buf.tm_wday == 0) ? 6 : (tm_buf.tm_wday - 1);
	int hhmm = tm_buf.tm_hour * 100 + tm_buf.tm_min;

	bool day_ok;
	if (acc->sched_day_from <= acc->sched_day_to)
		day_ok = (wday >= acc->sched_day_from && wday <= acc->sched_day_to);
	else
		day_ok = (wday >= acc->sched_day_from || wday <= acc->sched_day_to);

	if (!day_ok) {
		tcmg_log_dbg(D_NEWCAMD, "user=%s schedule day block: wday=%d allowed=%d-%d",
		             acc->user, wday, acc->sched_day_from, acc->sched_day_to);
		return false;
	}

	bool time_ok;
	if (acc->sched_hhmm_from <= acc->sched_hhmm_to)
		time_ok = (hhmm >= acc->sched_hhmm_from && hhmm < acc->sched_hhmm_to);
	else
		time_ok = (hhmm >= acc->sched_hhmm_from || hhmm < acc->sched_hhmm_to);

	if (!time_ok)
		tcmg_log_dbg(D_NEWCAMD, "user=%s schedule time block: hhmm=%04d allowed=%04d-%04d",
		             acc->user, hhmm, acc->sched_hhmm_from, acc->sched_hhmm_to);
	return time_ok;
}

static bool ncd_handle_login(S_CLIENT *cl,
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
		ncd_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: short packet (dlen=%d expected>=4)", ip, dlen);
		return false;
	}

	user = (const char *)(data + 3);
	umax = (size_t)(dlen - 3);
	ulen = strnlen(user, umax);
	if (ulen >= umax)
	{
		ncd_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: malformed user field (ulen=%u umax=%u)",
		         ip, (unsigned)ulen, (unsigned)umax);
		return false;
	}
	hash = user + ulen + 1;
	if ((hash - (const char *)data) >= dlen)
	{
		ncd_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: malformed hash field (offset=%ld dlen=%d)",
		         ip, (long)(hash - (const char *)data), dlen);
		return false;
	}

	tcmg_log_dbg(D_NEWCAMD, "%s LOGIN attempt user='%s'", ip, user);

	if (ban_is_banned(ip))
	{
		ncd_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: IP is banned", ip);
		return false;
	}

	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	acc = cfg_find_account(user);
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	if (!acc)
	{
		ncd_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: unknown user '%s'", ip, user);
		ban_record_fail(ip);
		return false;
	}
	if (!acc->enabled)
	{
		ncd_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: account disabled user='%s'", ip, user);
		return false;
	}

	if (acc->nwhitelist > 0)
	{
		bool ok = false;
		for (int j = 0; j < acc->nwhitelist; j++)
			if (strncmp(acc->ip_whitelist[j], ip, MAXIPLEN) == 0)
				{ ok = true; break; }
		if (!ok)
		{
			ncd_nak(cl, sid, mid, pid);
			tcmg_log("%s LOGIN failed: IP not in whitelist for user='%s' (whitelist has %d entries)",
			         ip, user, acc->nwhitelist);
			return false;
		}
	}

	if (!crypt_md5_crypt(acc->pass, hash, expected, sizeof(expected)) ||
	    !ct_streq(expected, hash))
	{
		ncd_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: wrong password for user='%s'", ip, user);
		ban_record_fail(ip);
		return false;
	}

	if (acc->expirationdate > 0 && time(NULL) > acc->expirationdate)
	{
		ncd_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: account expired user='%s' expired=%ld",
		         ip, acc->user, (long)acc->expirationdate);
		return false;
	}

	if (acc->max_connections > 0)
	{
		pthread_rwlock_wrlock(&g_cfg.acc_lock);
		bool over = ((int32_t)acc->active >= acc->max_connections);
		if (!over)
			atomic_fetch_add(&acc->active, 1);
		pthread_rwlock_unlock(&g_cfg.acc_lock);
		if (over)
		{
			ncd_nak(cl, sid, mid, pid);
			tcmg_log("%s LOGIN failed: max_connections=%d reached for user='%s' active=%d",
			         ip, acc->max_connections, acc->user, (int)acc->active);
			return false;
		}
	}
	else
	{
		atomic_fetch_add(&acc->active, 1);
	}

	{ uint8_t r[3] = { MSG_CLIENT_LOGIN_ACK, 0, 0 };
	  nc_send(cl, r, 3, sid, mid, pid); }

	{ size_t hlen = strlen(hash);
	  for (i = 0; i < (int)hlen; i++)
	      cl->session_key[i % 14] ^= (uint8_t)hash[i]; }
	crypt_key_spread(cl->session_key, spread);
	memcpy(cl->key1, spread,     8);
	memcpy(cl->key2, spread + 8, 8);
	secure_zero(spread, sizeof(spread));

	cl->caid      = acc->caid;
	cl->client_id = sid;
	cl->is_mgcamd = (g_cfg.newcamd_mgclient || acc->ncaids > 0) ? 1 : 0;
	tcmg_strlcpy(cl->proto, cl->is_mgcamd ? "mgcamd" : "newcamd", sizeof(cl->proto));
	tcmg_strlcpy(cl->user,        acc->user,            CFGKEY_LEN);
	tcmg_strlcpy(cl->client_name, cfg_client_name(sid), sizeof(cl->client_name));
	cl->account = acc;

	log_set_user(acc->user);

	atomic_store(&acc->last_seen, time(NULL));
	if (acc->first_login == 0) acc->first_login = time(NULL);
	ban_record_ok(ip);

	if (cl->is_mgcamd)
	{
		char caids[64];
		int pos = snprintf(caids, sizeof(caids), "%04X", acc->caid);
		for (i = 0; i < acc->ncaids; i++)
			pos += snprintf(caids + pos, sizeof(caids) - pos,
			                ",%04X", acc->caids[i]);
		tcmg_log("%s [mgcamd] LOGIN ok user='%s' caids=[%s] max_conn=%d",
		         ip, user, caids, acc->max_connections);
	}
	else
	{
		tcmg_log("%s [newcamd] LOGIN ok user='%s' caid=%04X max_conn=%d",
		         ip, user, acc->caid, acc->max_connections);
	}
	return true;
}

static void ncd_handle_card(S_CLIENT *cl, uint16_t sid, uint16_t mid, uint32_t pid)
{
	uint8_t  resp[26];
	uint16_t caid = cl->account ? cl->account->caid : cl->caid;

	memset(resp, 0, sizeof(resp));
	resp[0] = MSG_CARD_DATA;
	resp[4] = (uint8_t)(caid >> 8);
	resp[5] = (uint8_t)(caid & 0xFF);
	nc_send(cl, resp, 26, sid, mid, pid);
	tcmg_log_dbg(D_NEWCAMD, "%s CARD_DATA user='%s' caid=%04X sid=%04X",
	             cl->ip, cl->user, caid, sid);

	if (cl->is_mgcamd && cl->account)
	{
		tcmg_log_dbg(D_NEWCAMD, "%s [mgcamd] sending ADDCARD for %d caid(s)",
		             cl->ip, cl->account->ncaids + 1);
		nc_send_addcard(cl, caid, 0, mid);
		for (int i = 0; i < cl->account->ncaids; i++)
			if (cl->account->caids[i] != caid)
				nc_send_addcard(cl, cl->account->caids[i], 0, mid);
	}
}

static void ncd_handle_ecm(S_CLIENT *cl, uint8_t cmd,
                             const uint8_t *data, int32_t dlen,
                             uint16_t sid, uint16_t mid, uint32_t pid,
                             uint16_t caid_hdr)
{
	uint8_t   resp[32], cw[CW_LEN];
	int32_t   res;
	uint16_t  ecm_caid;
	S_ECM_CTX ctx;
	int64_t   t0_ms = 0;
	long      ms    = 0;

	memset(cw, 0, CW_LEN);

	if (!cl->account)
	{
		tcmg_log_dbg(D_NEWCAMD, "%s ECM denied: no account context", cl->ip);
		ncd_ecm_nak(cl, cmd, sid, mid, pid);
		return;
	}

	if (!account_in_schedule(cl->account))
	{
		tcmg_log("%s ECM denied: outside schedule for user='%s'", cl->ip, cl->user);
		ncd_ecm_nak(cl, cmd, sid, mid, pid);
		return;
	}

	ecm_caid = cl->caid;
	if (cl->is_mgcamd && caid_hdr)
	{
		bool ok = (cl->account->caid == caid_hdr);
		if (!ok)
			for (int i = 0; i < cl->account->ncaids; i++)
				if (cl->account->caids[i] == caid_hdr) { ok = true; break; }
		if (!ok)
		{
			tcmg_log("%s ECM denied: caid=%04X not permitted for user='%s'",
			         cl->ip, caid_hdr, cl->user);
			ncd_ecm_nak(cl, cmd, sid, mid, pid);
			return;
		}
		ecm_caid = caid_hdr;
	}

	if (cl->account->nsid_whitelist > 0)
	{
		bool sid_ok = false;
		for (int i = 0; i < cl->account->nsid_whitelist; i++)
			if (cl->account->sid_whitelist[i] == sid) { sid_ok = true; break; }
		if (!sid_ok)
		{
			tcmg_log_dbg(D_NEWCAMD, "%s ECM denied: sid=%04X not in whitelist for user='%s' (list has %d entries)",
			             cl->ip, sid, cl->user, cl->account->nsid_whitelist);
			ncd_ecm_nak(cl, cmd, sid, mid, pid);
			return;
		}
	}

	cl->last_ecm_time = time(NULL);
	cl->last_caid     = ecm_caid;
	cl->last_srvid    = sid;
	srvid_lookup_copy(ecm_caid, sid, cl->last_channel, sizeof(cl->last_channel));

	tcmg_log_dbg(D_ECM, "%s ECM request user='%s' caid=%04X sid=%04X dlen=%d channel='%s'",
	             cl->ip, cl->user, ecm_caid, sid, dlen,
	             cl->last_channel[0] ? cl->last_channel : "unknown");

	if (D_ECM & g_dblevel)
		log_ecm_raw(ecm_caid, sid, data, dlen);

	tcmg_strlcpy(ctx.user, cl->user, CFGKEY_LEN);
	tcmg_strlcpy(ctx.ip,   cl->ip,  MAXIPLEN);
	ctx.fd        = cl->fd;
	ctx.caid      = ecm_caid;
	ctx.thread_id = cl->thread_id;
	ctx.account   = cl->account;

	uint8_t ecm_md5[16];
	crypt_md5_hash(data, (size_t)dlen, ecm_md5);
	bool cache_hit = cw_cache_lookup(ecm_md5, cw);

	t0_ms = tcmg_mono_ms();

	if (cache_hit)
	{
		tcmg_log_dbg(D_ECM, "%s ECM cache HIT user='%s' caid=%04X sid=%04X",
		             cl->ip, cl->user, ecm_caid, sid);
		res = EMU_OK;
	}
	else
	{
		res = emu_process(ecm_caid, sid, data, dlen, cw, &ctx);
		if (res == EMU_OK)
			cw_cache_store(ecm_md5, cw);
	}

	ms = (long)tcmg_elapsed_ms(t0_ms);

	resp[0] = cmd;
	if (res == EMU_OK)
	{
		resp[1] = 0;
		resp[2] = CW_LEN;
		memcpy(resp + 3, cw, CW_LEN);
		nc_send(cl, resp, 19, sid, mid, pid);
		atomic_store(&cl->account->last_seen, time(NULL));

		pthread_mutex_lock(&cl->account->stat_mtx);
		cl->account->cw_found++;
		cl->account->ecm_total++;
		cl->account->cw_time_total_ms += ms;
		if (cl->account->cw_time_min_ms == 0 || ms < cl->account->cw_time_min_ms)
			cl->account->cw_time_min_ms = ms;
		if (ms > cl->account->cw_time_max_ms)
			cl->account->cw_time_max_ms = ms;
		pthread_mutex_unlock(&cl->account->stat_mtx);
	}
	else
	{
		resp[1] = resp[2] = 0;
		nc_send(cl, resp, 3, sid, mid, pid);

		pthread_mutex_lock(&cl->account->stat_mtx);
		cl->account->cw_not++;
		cl->account->ecm_total++;
		pthread_mutex_unlock(&cl->account->stat_mtx);
	}

	log_cw_result(ecm_caid, sid, dlen, cw, res == EMU_OK, cache_hit, (int32_t)ms, cl->user);
	secure_zero(cw, sizeof(cw));
}

void *handle_newcamd_client(void *arg)
{
	S_CONN_ARGS *args = (S_CONN_ARGS *)arg;
	S_CLIENT     cl;
	uint8_t      data[NC_MSG_MAX];
	uint16_t     sid, mid, caid_hdr;
	uint32_t     pid;
	int32_t      dlen;

	memset(&cl, 0, sizeof(cl));
	cl.fd           = args->fd;
	cl.thread_id    = (uint32_t)(uintptr_t)pthread_self();
	cl.connect_time = time(NULL);
	cl.last_ecm_time = time(NULL);
	tcmg_strlcpy(cl.ip, args->ip, MAXIPLEN);
	free(args);

	log_set_type(LOG_TYPE_CLIENT);
	client_register(&cl);
	tcmg_log_dbg(D_CONN, "%s new newcamd/mgcamd connection fd=%d tid=%u",
	             cl.ip, cl.fd, cl.thread_id);

	nc_init(&cl, g_cfg.newcamd_key, g_cfg.sock_timeout);

	while (g_running && !cl.kill_flag)
	{
		if (cl.account && cl.account->max_idle > 0)
		{
			time_t idle = time(NULL) - cl.last_ecm_time;
			if (idle >= cl.account->max_idle)
			{
				tcmg_log("%s idle timeout: %lds >= max_idle=%ds disconnecting user='%s'",
				         cl.ip, (long)idle, cl.account->max_idle, cl.user);
				break;
			}
		}

		dlen = nc_recv(&cl, data, &sid, &mid, &pid, &caid_hdr);
		if (dlen < 0)
		{
			if (cl.user[0])
				tcmg_log("%s disconnected user='%s' ecm_total=%llu cw_found=%lld cw_not=%lld",
				         cl.ip, cl.user,
				         cl.account ? (unsigned long long)cl.account->ecm_total : 0ULL,
				         cl.account ? (long long)cl.account->cw_found : 0LL,
				         cl.account ? (long long)cl.account->cw_not : 0LL);
			else
				tcmg_log_dbg(D_CONN, "%s disconnected (before login)", cl.ip);
			break;
		}

		uint8_t cmd = data[0];
		tcmg_log_dbg(D_NEWCAMD, "%s recv cmd=0x%02X dlen=%d sid=%04X mid=%04X",
		             cl.ip, cmd, dlen, sid, mid);

		if      (cmd == MSG_CLIENT_LOGIN)
		{ if (!ncd_handle_login(&cl, data, dlen, sid, mid, pid)) break; }
		else if (cmd == MSG_CARD_DATA_REQ)
		{ ncd_handle_card(&cl, sid, mid, pid); }
		else if (cmd == MSG_KEEPALIVE)
		{
			tcmg_log_dbg(D_NEWCAMD, "%s KEEPALIVE user='%s'", cl.ip, cl.user);
			if (g_cfg.newcamd_keepalive)
				nc_send(&cl, data, dlen, sid, mid, pid);
		}
		else if (cmd == MSG_ECM_0 || cmd == MSG_ECM_1)
		{ ncd_handle_ecm(&cl, cmd, data, dlen, sid, mid, pid, caid_hdr); }
		else if (cmd == MSG_GET_VERSION)
		{
			tcmg_log_dbg(D_NEWCAMD, "%s GET_VERSION request", cl.ip);
			nc_send_version(&cl, mid);
		}
		else
		{
			tcmg_log_dbg(D_NEWCAMD, "%s unknown cmd=0x%02X dlen=%d -- ignored",
			             cl.ip, cmd, dlen);
		}
	}

	client_unregister(&cl);
	if (cl.account)
		atomic_fetch_sub(&cl.account->active, 1);

	tcmg_log_dbg(D_CONN, "%s connection closed fd=%d tid=%u", cl.ip, cl.fd, cl.thread_id);
	close(cl.fd);
	atomic_fetch_sub(&g_active_conns, 1);
	return NULL;
}

static void *ncd_accept_thread(void *arg)
{
	(void)arg;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setstacksize(&attr, 256 * 1024);

	while (s_ncd_running)
	{
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(s_ncd_srv_fd, &rfds);
		struct timeval tv = { 1, 0 };
		if (select(s_ncd_srv_fd + 1, &rfds, NULL, NULL, &tv) <= 0)
			continue;

		struct sockaddr_in ca;
		socklen_t clen = sizeof(ca);
		int cfd = (int)accept(s_ncd_srv_fd, (struct sockaddr *)&ca, &clen);
		if (cfd < 0)
		{
			if (s_ncd_running)
				tcmg_log_dbg(D_CONN, "accept() failed errno=%d (%s)",
				             errno, strerror(errno));
			continue;
		}

		int active = atomic_fetch_add(&g_active_conns, 1);
		if (active >= MAX_CONNS)
		{
			atomic_fetch_sub(&g_active_conns, 1);
			close(cfd);
			tcmg_log("MAX_CONNS=%d reached -- connection rejected active=%d",
			         MAX_CONNS, active);
			continue;
		}

		S_CONN_ARGS *args = (S_CONN_ARGS *)malloc(sizeof(S_CONN_ARGS));
		if (!args)
		{
			atomic_fetch_sub(&g_active_conns, 1);
			close(cfd);
			tcmg_log("out of memory -- connection rejected active=%d", active);
			continue;
		}
		args->fd = cfd;
		inet_ntop(AF_INET, &ca.sin_addr, args->ip, MAXIPLEN);
		args->ip[MAXIPLEN - 1] = '\0';

		tcmg_log_dbg(D_CONN, "%s accepted newcamd connection fd=%d active=%d",
		             args->ip, cfd, active + 1);

		pthread_t tid;
		int rc = pthread_create(&tid, &attr, handle_newcamd_client, args);
		if (rc != 0)
		{
			tcmg_log("pthread_create failed: rc=%d errno=%d (%s)",
			         rc, errno, strerror(errno));
			atomic_fetch_sub(&g_active_conns, 1);
			close(cfd);
			free(args);
		}
	}

	pthread_attr_destroy(&attr);
	tcmg_log_dbg(D_CONN, "%s", "accept thread exiting");
	return NULL;
}

int32_t newcamd_start(void)
{
	if (!g_cfg.newcamd_port) {
		tcmg_log_dbg(D_NEWCAMD, "%s", "disabled (port=0)");
		return -1;
	}

	s_ncd_srv_fd = (int)socket(AF_INET, SOCK_STREAM, 0);
	if (s_ncd_srv_fd < 0)
	{
		tcmg_log("socket() failed errno=%d (%s)", errno, strerror(errno));
		return -1;
	}

	{ int opt = 1;
	  setsockopt(s_ncd_srv_fd, SOL_SOCKET, SO_REUSEADDR, SO_CAST(&opt), sizeof(opt)); }

	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	if (g_cfg.newcamd_bindaddr[0])
	{
		inet_pton(AF_INET, g_cfg.newcamd_bindaddr, &sa.sin_addr);
		tcmg_log_dbg(D_NEWCAMD, "binding to %s:%d", g_cfg.newcamd_bindaddr, g_cfg.newcamd_port);
	}
	else
	{
		sa.sin_addr.s_addr = INADDR_ANY;
		tcmg_log_dbg(D_NEWCAMD, "binding to *:%d", g_cfg.newcamd_port);
	}
	sa.sin_port = htons((uint16_t)g_cfg.newcamd_port);

	if (bind(s_ncd_srv_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
	{
		tcmg_log("bind() failed port=%d errno=%d (%s)",
		         g_cfg.newcamd_port, errno, strerror(errno));
		close(s_ncd_srv_fd);
		s_ncd_srv_fd = -1;
		return -1;
	}
	if (listen(s_ncd_srv_fd, 128) < 0)
	{
		tcmg_log("listen() failed errno=%d (%s)", errno, strerror(errno));
		close(s_ncd_srv_fd);
		s_ncd_srv_fd = -1;
		return -1;
	}

	s_ncd_running = 1;
	int rc = pthread_create(&s_ncd_thread, NULL, ncd_accept_thread, NULL);
	if (rc != 0)
	{
		tcmg_log("pthread_create failed rc=%d errno=%d (%s)",
		         rc, errno, strerror(errno));
		s_ncd_running = 0;
		close(s_ncd_srv_fd);
		s_ncd_srv_fd = -1;
		return -1;
	}

	tcmg_log("listening on port=%d mgclient=%d keepalive=%d sock_timeout=%ds",
	         g_cfg.newcamd_port, g_cfg.newcamd_mgclient,
	         g_cfg.newcamd_keepalive, g_cfg.sock_timeout);
	return 0;
}

void newcamd_stop(void)
{
	tcmg_log_dbg(D_NEWCAMD, "%s", "stopping...");
	s_ncd_running = 0;
	if (s_ncd_srv_fd >= 0)
	{
		close(s_ncd_srv_fd);
		s_ncd_srv_fd = -1;
	}
	pthread_join(s_ncd_thread, NULL);
	tcmg_log_dbg(D_NEWCAMD, "%s", "stopped");
}
