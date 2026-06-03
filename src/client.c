#define MODULE_LOG_PREFIX "client"
#include "tcmg.h"
#include "client.h"

static bool cw_cache_lookup(const uint8_t *ecm_md5, uint8_t *cw_out)
{
	uint32_t idx = ((uint32_t)ecm_md5[0] | ((uint32_t)ecm_md5[1] << 8))
	               & (CW_CACHE_SIZE - 1);
	uint32_t shard = idx & (CW_CACHE_SHARDS - 1);
	bool hit = false;
	pthread_mutex_lock(&g_cw_cache_mtx[shard]);
	S_CW_CACHE_ENTRY *e = &g_cw_cache[idx];
	if (e->valid && ct_memeq(e->ecm_md5, ecm_md5, 16) &&
	    (time(NULL) - e->ts) < CW_CACHE_TTL_S)
	{
		memcpy(cw_out, e->cw, CW_LEN);
		hit = true;
	}
	pthread_mutex_unlock(&g_cw_cache_mtx[shard]);
	return hit;
}

static void cw_cache_store(const uint8_t *ecm_md5, const uint8_t *cw)
{
	uint32_t idx = ((uint32_t)ecm_md5[0] | ((uint32_t)ecm_md5[1] << 8))
	               & (CW_CACHE_SIZE - 1);
	uint32_t shard = idx & (CW_CACHE_SHARDS - 1);
	pthread_mutex_lock(&g_cw_cache_mtx[shard]);
	S_CW_CACHE_ENTRY *e = &g_cw_cache[idx];
	memcpy(e->ecm_md5, ecm_md5, 16);
	memcpy(e->cw,      cw,      CW_LEN);
	e->ts    = time(NULL);
	e->valid = 1;
	pthread_mutex_unlock(&g_cw_cache_mtx[shard]);
}

void client_register(S_CLIENT *cl)
{
	pthread_mutex_lock(&g_clients_mtx);
	for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++)
		if (!g_clients[i]) { g_clients[i] = cl; break; }
	pthread_mutex_unlock(&g_clients_mtx);
}

void client_unregister(S_CLIENT *cl)
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

void clients_relink_accounts(void)
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
		cl->account = a;        /* NULL if account was deleted → kill */
		if (!a) cl->kill_flag = 1;
	}
	pthread_rwlock_unlock(&g_cfg.acc_lock);
	pthread_mutex_unlock(&g_clients_mtx);
}

static void nc_nak(S_CLIENT *cl, uint16_t sid, uint16_t mid, uint32_t pid)
{
	uint8_t r[3] = { MSG_CLIENT_LOGIN_NAK, 0, 0 };
	nc_send(cl, r, 3, sid, mid, pid);
}

static void ecm_send_nak(S_CLIENT *cl, uint8_t cmd,
                          uint16_t sid, uint16_t mid, uint32_t pid)
{
	uint8_t r[3] = { cmd, 0, 0 };
	nc_send(cl, r, 3, sid, mid, pid);
}

/* Returns true if now falls inside the account's configured schedule window */
static bool account_in_schedule(const S_ACCOUNT *acc)
{
	if (acc->sched_day_from < 0) return true;

	time_t now = time(NULL);
	struct tm tm_buf;
	localtime_r(&now, &tm_buf);

	/* tm_wday: 0=Sun…6=Sat  →  normalise to Mon=0…Sun=6 */
	int wday = (tm_buf.tm_wday == 0) ? 6 : (tm_buf.tm_wday - 1);
	int hhmm = tm_buf.tm_hour * 100 + tm_buf.tm_min;

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

/* Login */
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
		bool ok = false;
		for (int j = 0; j < acc->nwhitelist; j++)
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
		return false;
	}

	if (acc->expirationdate > 0 && time(NULL) > acc->expirationdate)
	{
		nc_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: account '%s' expired", ip, acc->user);
		return false;
	}

	if (acc->max_connections > 0)
	{
		pthread_rwlock_wrlock(&g_cfg.acc_lock);
		bool over = ((int32_t)acc->active >= acc->max_connections);
		if (!over)
			__sync_fetch_and_add(&acc->active, 1);
		pthread_rwlock_unlock(&g_cfg.acc_lock);
		if (over)
		{
			nc_nak(cl, sid, mid, pid);
			tcmg_log("%s LOGIN failed: '%s' max_connections=%d reached",
			         ip, acc->user, acc->max_connections);
			return false;
		}
	}
	else
	{
		__sync_fetch_and_add(&acc->active, 1);
	}

	/* ACK */
	{ uint8_t r[3] = { MSG_CLIENT_LOGIN_ACK, 0, 0 };
	  nc_send(cl, r, 3, sid, mid, pid); }

	/* Derive session keys from the hash */
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
	tcmg_strlcpy(cl->user,        acc->user,            CFGKEY_LEN);
	tcmg_strlcpy(cl->client_name, cfg_client_name(sid), sizeof(cl->client_name));
	cl->account   = acc;

	log_set_user(acc->user);

	__sync_lock_test_and_set(&acc->last_seen, time(NULL));
	if (acc->first_login == 0) acc->first_login = time(NULL);
	ban_record_ok(ip);

	if (cl->is_mgcamd)
	{
		char caids[64];
		int pos = snprintf(caids, sizeof(caids), "%04X", acc->caid);
		for (i = 0; i < acc->ncaids; i++)
			pos += snprintf(caids + pos, sizeof(caids) - pos,
			                ",%04X", acc->caids[i]);
		tcmg_log("%s LOGIN ok: '%s' caids=[%s] client=%s", ip, user, caids, cl->client_name);
	}
	else
	{
		tcmg_log("%s LOGIN ok: '%s' caid=%04X client=%s", ip, user, acc->caid, cl->client_name);
	}
	tcmg_log_dbg(D_CLIENT, "%s authenticated '%s' caid=%04X", ip, user, acc->caid);
	return true;
}

/* Card-data */
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
		nc_send_addcard(cl, caid, 0, mid);
		for (int i = 0; i < cl->account->ncaids; i++)
			if (cl->account->caids[i] != caid)
				nc_send_addcard(cl, cl->account->caids[i], 0, mid);
	}
}

/* ECM */
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
		{ ecm_send_nak(cl, cmd, sid, mid, pid); return; }

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
			for (int i = 0; i < cl->account->ncaids; i++)
				if (cl->account->caids[i] == caid_hdr) { ok = true; break; }
		if (!ok)
		{
			tcmg_log("%s ECM denied: CAID %04X not permitted for '%s'", cl->ip, caid_hdr, cl->user);
			ecm_send_nak(cl, cmd, sid, mid, pid);
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
			tcmg_log_dbg(D_CLIENT, "%s SID %04X not in whitelist for '%s'",
			             cl->ip, sid, cl->user);
			ecm_send_nak(cl, cmd, sid, mid, pid);
			return;
		}
	}

	cl->last_ecm_time = time(NULL);
	cl->last_caid     = ecm_caid;
	cl->last_srvid    = sid;
	srvid_lookup_copy(ecm_caid, sid, cl->last_channel, sizeof(cl->last_channel));

	tcmg_strlcpy(ctx.user, cl->user, CFGKEY_LEN);
	tcmg_strlcpy(ctx.ip,   cl->ip,  MAXIPLEN);
	ctx.fd        = cl->fd;
	ctx.caid      = ecm_caid;
	ctx.thread_id = cl->thread_id;
	ctx.account   = cl->account;

	uint8_t ecm_md5[16];
	crypt_md5_hash(data, (size_t)dlen, ecm_md5);
	bool cache_hit = cw_cache_lookup(ecm_md5, cw);

	if (cache_hit)
	{
		tcmg_log_dbg(D_ECM, "%s ECM cache HIT CAID=%04X SID=%04X",
		             cl->ip, ecm_caid, sid);
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
		__sync_lock_test_and_set(&cl->account->last_seen, time(NULL));
	}
	else
	{
		resp[1] = resp[2] = 0;
		nc_send(cl, resp, 3, sid, mid, pid);
	}
	secure_zero(cw, sizeof(cw));
}

void *handle_client(void *arg)
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
	tcmg_strlcpy(cl.ip, args->ip, MAXIPLEN);
	free(args);

	log_set_type(LOG_TYPE_CLIENT);
	client_register(&cl);
	tcmg_log("%s connected [%d active]", cl.ip, g_active_conns);
	tcmg_log_dbg(D_CLIENT, "%s new connection", cl.ip);

	nc_init(&cl, g_cfg.des_key, g_cfg.sock_timeout);

	while (g_running && !cl.kill_flag)
	{
		if (cl.account && cl.account->max_idle > 0)
		{
			time_t idle = time(NULL) - cl.last_ecm_time;
			if (idle >= cl.account->max_idle)
			{
				tcmg_log("%s idle timeout: %lds >= %ds, disconnecting '%s'",
				         cl.ip, (long)idle, cl.account->max_idle, cl.user);
				break;
			}
		}

		dlen = nc_recv(&cl, data, &sid, &mid, &pid, &caid_hdr);
		if (dlen < 0)
		{
			tcmg_log("%s disconnected", cl.ip);
			break;
		}

		uint8_t cmd = data[0];

		if      (cmd == MSG_CLIENT_LOGIN)
		{ if (!handle_login(&cl, data, dlen, sid, mid, pid)) break; }
		else if (cmd == MSG_CARD_DATA_REQ)
		{ handle_card(&cl, sid, mid, pid); }
		else if (cmd == MSG_KEEPALIVE)
		{ nc_send(&cl, data, dlen, sid, mid, pid); }
		else if (cmd == MSG_ECM_0 || cmd == MSG_ECM_1)
		{ handle_ecm(&cl, cmd, data, dlen, sid, mid, pid, caid_hdr); }
		else if (cmd == MSG_GET_VERSION)
		{ nc_send_version(&cl, mid); }
		else
		{ tcmg_log_dbg(D_PROTO, "%s unknown cmd=0x%02X", cl.ip, cmd); }
	}

	client_unregister(&cl);
	if (cl.account)
		__sync_fetch_and_sub(&cl.account->active, 1);

	close(cl.fd);
	__sync_fetch_and_sub(&g_active_conns, 1);
	return NULL;
}
