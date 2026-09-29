#include "stats/account_stats.h"
#define MODULE_LOG_PREFIX "newcamd"
#include "../core/config_state.h"
#include "../config/runtime_access.h"
#include "../core/runtime_state.h"
#include "../core/utils.h"
#include "../config/config.h"
#include "../platform/platform.h"
#include "../log/log.h"
#include "../net/net.h"
#include "../crypto/crypto.h"
#include "../crypto/newcamd_des.h"
#include "../security/failban.h"
#include "reader/reader.h"
#include "account/account.h"
#include "ecm/ecm.h"
#include "session/session.h"
#include "newcamd.h"
#include "server.h"

static S_PROTO_SERVER s_server;

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

static bool ncd_handle_login(S_CLIENT *cl,
                              const uint8_t *data, int32_t dlen,
                              uint16_t sid, uint16_t mid, uint32_t pid,
                              const S_CONFIG_NETWORK_VIEW *netcfg)
{
	const char *ip = cl->identity.ip;
	S_ACCOUNT  *acc;
	const char *user, *hash;
	size_t      umax, ulen;
	char        expected[64];
	uint8_t     key16[16];

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

	tcmg_log_dbg(D_NEWCAMD, "%s LOGIN attempt user='%s' proto=%u mg=%d",
	             ip, user, (unsigned)cl->protocol.wire.newcamd.proto, (int)cl->protocol.wire.newcamd.is_mgcamd);

	if (ban_is_banned(ip))
	{
		ncd_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: IP is banned", ip);
		return false;
	}

	acc = account_acquire(user);

	if (!acc)
	{
		ncd_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: unknown user '%s'", ip, user);
		ban_record_fail(ip);
		return false;
	}
	T_ACCOUNT_STATUS account_status = account_validate(acc, ip);
	if (account_status != ACCOUNT_OK)
	{
		ncd_nak(cl, sid, mid, pid);
		switch (account_status) {
		case ACCOUNT_DISABLED:
			tcmg_log("%s LOGIN failed: account disabled user='%s'", ip, user);
			break;
		case ACCOUNT_EXPIRED:
			tcmg_log("%s LOGIN failed: account expired user='%s' expired=%ld",
			         ip, acc->user, (long)acc->expirationdate);
			break;
		case ACCOUNT_IP_DENIED:
			tcmg_log("%s LOGIN failed: IP not in whitelist for user='%s' (whitelist has %d entries)",
			         ip, user, acc->nwhitelist);
			break;
		default:
			break;
		}
		account_release(acc);
		return false;
	}

	if (!crypt_md5_crypt(acc->pass, hash, expected, sizeof(expected)) ||
	    !ct_streq(expected, hash))
	{
		ncd_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: wrong password for user='%s'", ip, user);
		ban_record_fail(ip);
		account_release(acc);
		return false;
	}

	if (account_session_open(cl, acc) < 0)
	{
		ncd_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: max_connections=%d reached for user='%s' active=%d",
		         ip, acc->max_connections, acc->user, (int)acc->active);
		account_release(acc);
		return false;
	}

	{ uint8_t r[3] = { MSG_CLIENT_LOGIN_ACK, 0, 0 };
	  nc_send(cl, r, 3, sid, mid, pid); }

	{ size_t hlen = strlen(hash);
	  if (hlen == 0 || hlen > NC_MSG_MAX) return false;
	  tcmg_ncd_des_login_key_get(netcfg->newcamd_key, (const uint8_t *)hash, (int)hlen, key16);
	  memcpy(cl->protocol.wire.newcamd.key1, key16, 8);
	  memcpy(cl->protocol.wire.newcamd.key2, key16 + 8, 8);
	}
	secure_zero(key16, sizeof(key16));

	pthread_mutex_lock(&cl->state_mtx);
	cl->ecm.caid = account_default_caid(acc);
	cl->identity.client_id = sid;
	cl->protocol.wire.newcamd.is_mgcamd = (cl->protocol.wire.newcamd.is_mgcamd || (netcfg->newcamd_mgclient != 0)) ? 1 : 0;
	tcmg_strlcpy(cl->protocol.name, cl->protocol.wire.newcamd.is_mgcamd ? "mgcamd" : "newcamd", sizeof(cl->protocol.name));
	tcmg_strlcpy(cl->identity.user, acc->user, sizeof(cl->identity.user));
	tcmg_strlcpy(cl->identity.client_name, cfg_client_name(sid), sizeof(cl->identity.client_name));
	pthread_mutex_unlock(&cl->state_mtx);
	account_mark_login(acc, ip);
	ban_record_ok(ip);

	if (cl->protocol.wire.newcamd.is_mgcamd)
	{
		uint16_t login_caids[MAX_CAIDS_PER_ACC + (MAX_READERS * MAX_CAIDS_PER_READER)];
		int32_t login_ncaids = account_collect_caids(acc, login_caids,
		                                               (int32_t)(sizeof(login_caids) / sizeof(login_caids[0])));
		char caids[256];
		int pos = 0;
		for (int32_t j = 0; j < login_ncaids && pos < (int)sizeof(caids); j++)
			pos += snprintf(caids + pos, sizeof(caids) - (size_t)pos,
			                "%s%04X", j ? "," : "", login_caids[j]);
		tcmg_log("%s [mgcamd] LOGIN ok user='%s' caids=[%s] max_conn=%d",
		         ip, user, login_ncaids ? caids : "none", acc->max_connections);
	}
	else
	{
		tcmg_log("%s [newcamd] LOGIN ok user='%s' caid=%04X max_conn=%d",
		         ip, user, cl->ecm.caid, acc->max_connections);
	}
	return true;
}

static void ncd_handle_card(S_CLIENT *cl, uint16_t sid, uint16_t mid, uint32_t pid)
{
    uint8_t resp[26];
    S_ACCOUNT *account = account_session_acquire(cl);
    uint16_t caids[MAX_CAIDS_PER_ACC + (MAX_READERS * MAX_CAIDS_PER_READER)];
    int32_t ncaids = account ? account_collect_caids(account, caids,
                                                     (int32_t)(sizeof(caids) / sizeof(caids[0]))) : 0;
    uint16_t caid = ncaids > 0 ? caids[0] : cl->ecm.caid;

    memset(resp, 0, sizeof(resp));
    resp[0] = MSG_CARD_DATA;
    resp[4] = (uint8_t)(caid >> 8);
    resp[5] = (uint8_t)(caid & 0xFF);
    nc_send(cl, resp, 26, sid, mid, pid);
    tcmg_log_dbg(D_NEWCAMD, "%s CARD_DATA user='%s' caid=%04X sid=%04X",
                 cl->identity.ip, cl->identity.user, caid, sid);

    if (account && cl->protocol.wire.newcamd.is_mgcamd) {
        tcmg_log_dbg(D_NEWCAMD, "%s [mgcamd] sending ADDCARD for %d caid(s)",
                     cl->identity.ip, ncaids);
        for (int32_t i = 0; i < ncaids; i++)
            nc_send_addcard(cl, caids[i], 0, mid);
    }
    if (account) account_release(account);
}

static void ncd_handle_ecm(S_CLIENT *cl, uint8_t cmd,
                             const uint8_t *data, int32_t dlen,
                             uint16_t sid, uint16_t mid, uint32_t pid,
                             uint16_t caid_hdr)
{
    uint8_t resp[32] = {0};
    uint8_t cw[CW_LEN] = {0};
    uint16_t ecm_caid = cl ? cl->ecm.caid : 0;
    T_ECM_ACCESS_STATUS access;
    S_ECM_RESULT result;

    if (!cl) {
        return;
    }
    if (dlen <= 0) {
        ncd_ecm_nak(cl, cmd, sid, mid, pid);
        return;
    }

    if (cl->protocol.wire.newcamd.is_mgcamd && caid_hdr)
        ecm_caid = caid_hdr;

    access = ecm_access(cl, ecm_caid, sid, true, true, true);
    if (access != ECM_ACCESS_OK) {
        switch (access) {
        case ECM_ACCESS_DISABLED:
            tcmg_log("%s ECM denied: account disabled mid-session user='%s'", cl->identity.ip, cl->identity.user);
            break;
        case ECM_ACCESS_EXPIRED: {
            S_ACCOUNT *ea = account_session_acquire(cl);
            long exp = ea ? (long)ea->expirationdate : 0L;
            tcmg_log("%s ECM denied: account expired mid-session user='%s' expired=%ld",
                     cl->identity.ip, cl->identity.user, exp);
            if (ea) account_release(ea);
            break;
        }
        case ECM_ACCESS_SCHEDULE:
            tcmg_log("%s ECM denied: outside schedule for user='%s'", cl->identity.ip, cl->identity.user);
            break;
        case ECM_ACCESS_ANTISHARE:
            tcmg_log("%s ECM denied: anti-sharing limit triggered for user='%s' sid=%04X",
                     cl->identity.ip, cl->identity.user, sid);
            break;
        case ECM_ACCESS_CAID_DENIED:
            tcmg_log("%s ECM denied: caid=%04X not permitted for user='%s'",
                     cl->identity.ip, ecm_caid, cl->identity.user);
            break;
        case ECM_ACCESS_SID_DENIED:
            tcmg_log_dbg(D_NEWCAMD, "%s ECM denied: sid=%04X not in whitelist for user='%s'",
                         cl->identity.ip, sid, cl->identity.user);
            break;
        default:
            tcmg_log_dbg(D_NEWCAMD, "%s ECM denied: no account context", cl->identity.ip);
            break;
        }
        ncd_ecm_nak(cl, cmd, sid, mid, pid);
        return;
    }

    tcmg_log_dbg(D_ECM, "%s ECM request user='%s' caid=%04X sid=%04X dlen=%d channel='%s'",
                 cl->identity.ip, cl->identity.user, ecm_caid, sid, dlen,
                 cl->ecm.last_channel[0] ? cl->ecm.last_channel : "unknown");

    if (ecm_process(cl, ecm_caid, sid, pid, data, dlen, cw, &result) < 0) {
        resp[0] = cmd;
        resp[1] = resp[2] = 0;
        nc_send(cl, resp, 3, sid, mid, pid);
        secure_zero(cw, sizeof(cw));
        return;
    }

    resp[0] = cmd;
    resp[1] = 0;
    resp[2] = CW_LEN;
    memcpy(resp + 3, cw, CW_LEN);
    nc_send(cl, resp, 19, sid, mid, pid);
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

	{
		T_SESSION_INPUT input = { args->fd, args->ip };
		session_init(&cl, &input, "newcamd");
	}
	free(args);
	S_CONFIG_NETWORK_VIEW netcfg;
	if (!cfg_runtime_network_snapshot(&netcfg)) {
		session_cleanup(&cl);
		return NULL;
	}
	tcmg_log_dbg(D_CONN, "%s new newcamd/mgcamd connection fd=%d tid=%u",
	             cl.identity.ip, cl.session.fd, cl.identity.thread_id);

	{
		int recv_timeout = netcfg.sock_timeout;
		if (netcfg.server_keepalive > 0 && netcfg.server_keepalive < recv_timeout)
			recv_timeout = netcfg.server_keepalive;
		nc_init(&cl, netcfg.newcamd_key, recv_timeout);
	}

	int ka_misses = 0;
	while (g_running && !cl.session.kill_flag)
	{
		if (session_idle_expired(&cl, time(NULL)))
		{
            time_t idle = time(NULL) - (cl.session.last_activity ? cl.session.last_activity : cl.ecm.last_ecm_time);
            S_ACCOUNT *ia = account_session_acquire(&cl);
            int max_idle = ia ? ia->max_idle : 0;
            tcmg_log("%s idle timeout: %lds >= max_idle=%ds disconnecting user='%s'",
                     cl.identity.ip, (long)idle, max_idle, cl.identity.user);
            if (ia) account_release(ia);
			break;
		}

		dlen = nc_recv(&cl, data, &sid, &mid, &pid, &caid_hdr);
		if (dlen == NET_RECV_TIMEOUT)
		{
            S_ACCOUNT *ka_account = account_session_acquire(&cl);
            bool logged_in = ka_account != NULL;
            if (ka_account) account_release(ka_account);
            if (logged_in && netcfg.server_keepalive > 0)
			{
				uint8_t ka[3] = { MSG_KEEPALIVE, 0, 0 };
				if (nc_send(&cl, ka, sizeof(ka), cl.ecm.last_srvid, 0, 0) < 0) break;
				ka_misses++;
				tcmg_log_dbg(D_NEWCAMD, "%s SERVER_KEEPALIVE user='%s' miss=%d/%d",
				             cl.identity.ip, cl.identity.user, ka_misses, netcfg.server_keepalive_misses);
				if (ka_misses >= netcfg.server_keepalive_misses) break;
				continue;
			}
			break;
		}
		if (dlen < 0)
		{
			if (cl.identity.user[0]) {
				S_ACCOUNT_STATS_SNAPSHOT stats;
                S_ACCOUNT *sa = account_session_acquire(&cl);
                if (sa) { account_stats_snapshot(sa, &stats); account_release(sa); }
				tcmg_log("%s disconnected user='%s' ecm_total=%llu cw_found=%lld cw_not=%lld",
				         cl.identity.ip, cl.identity.user,
				         (unsigned long long)stats.ecm_total,
				         (long long)stats.cw_found,
				         (long long)stats.cw_not);
			}
			else
				tcmg_log_dbg(D_CONN, "%s disconnected (before login)", cl.identity.ip);
			break;
		}

		ka_misses = 0;
		cl.session.last_activity = time(NULL);
		uint8_t cmd = data[0];
		tcmg_log_dbg(D_NEWCAMD, "%s recv cmd=0x%02X dlen=%d sid=%04X mid=%04X",
		             cl.identity.ip, cmd, dlen, sid, mid);

        S_ACCOUNT *cmd_account = account_session_acquire(&cl);
        bool logged_in = cmd_account != NULL;
        if (cmd_account) account_release(cmd_account);
        if (!logged_in && cmd != MSG_CLIENT_LOGIN)
		{
			tcmg_log_dbg(D_NEWCAMD, "%s command 0x%02X rejected before authentication", cl.identity.ip, cmd);
			ncd_ecm_nak(&cl, cmd, sid, mid, pid);
			break;
		}
        if (logged_in && cmd == MSG_CLIENT_LOGIN)
		{
			tcmg_log_dbg(D_NEWCAMD, "%s repeated login rejected for user='%s'", cl.identity.ip, cl.identity.user);
			ncd_ecm_nak(&cl, cmd, sid, mid, pid);
			break;
		}

		if      (cmd == MSG_CLIENT_LOGIN)
		{ if (!ncd_handle_login(&cl, data, dlen, sid, mid, pid, &netcfg)) break; }
		else if (cmd == MSG_CARD_DATA_REQ)
		{ ncd_handle_card(&cl, sid, mid, pid); }
		else if (cmd == MSG_KEEPALIVE)
		{
			tcmg_log_dbg(D_NEWCAMD, "%s KEEPALIVE user='%s'", cl.identity.ip, cl.identity.user);
			if (netcfg.newcamd_keepalive)
				nc_send(&cl, data, dlen, sid, mid, pid);
		}
		else if (cmd == MSG_ECM_0 || cmd == MSG_ECM_1)
		{ ncd_handle_ecm(&cl, cmd, data, dlen, sid, mid, pid, caid_hdr); }
		else if (cmd == MSG_GET_VERSION)
		{
			tcmg_log_dbg(D_NEWCAMD, "%s GET_VERSION request", cl.identity.ip);
			nc_send_version(&cl, mid);
		}
		else
		{
			tcmg_log_dbg(D_NEWCAMD, "%s unknown cmd=0x%02X dlen=%d -- ignored",
			             cl.identity.ip, cmd, dlen);
		}
	}

	tcmg_log_dbg(D_CONN, "%s connection closed fd=%d tid=%u", cl.identity.ip, cl.session.fd, cl.identity.thread_id);
	session_cleanup(&cl);
	return NULL;
}

int32_t newcamd_start(void)
{
    return proto_server_start(&s_server, "newcamd", g_cfg.newcamd_port,
                               g_cfg.newcamd_bindaddr,
                               handle_newcamd_client);
}

void newcamd_stop(void)
{
    proto_server_stop(&s_server);
}
