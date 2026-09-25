#include "stats/account_stats.h"
#define MODULE_LOG_PREFIX "newcamd"
#include "../core/config_state.h"
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
                              uint16_t sid, uint16_t mid, uint32_t pid)
{
	const char *ip = cl->identity.ip;
	S_ACCOUNT  *acc;
	const char *user, *hash;
	size_t      umax, ulen;
	char        expected[64];
	uint8_t     key16[16];
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

	tcmg_log_dbg(D_NEWCAMD, "%s LOGIN attempt user='%s' proto=%u mg=%d",
	             ip, user, (unsigned)cl->protocol.wire.newcamd.proto, (int)cl->protocol.wire.newcamd.is_mgcamd);

	if (ban_is_banned(ip))
	{
		ncd_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: IP is banned", ip);
		return false;
	}

	acc = account_acquire(user);
	cl->auth.account = acc;

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
		return false;
	}

	if (!crypt_md5_crypt(acc->pass, hash, expected, sizeof(expected)) ||
	    !ct_streq(expected, hash))
	{
		ncd_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: wrong password for user='%s'", ip, user);
		ban_record_fail(ip);
		return false;
	}

	if (account_session_open(cl, acc) < 0)
	{
		ncd_nak(cl, sid, mid, pid);
		tcmg_log("%s LOGIN failed: max_connections=%d reached for user='%s' active=%d",
		         ip, acc->max_connections, acc->user, (int)acc->active);
		return false;
	}

	{ uint8_t r[3] = { MSG_CLIENT_LOGIN_ACK, 0, 0 };
	  nc_send(cl, r, 3, sid, mid, pid); }

	/* LOGIN_ACK is protected by the initial key.  Only after it is sent do
	 * both sides switch to the password-derived session key, matching OSCam. */
	{ size_t hlen = strlen(hash);
	  if (hlen == 0 || hlen > NC_MSG_MAX) return false;
	  tcmg_ncd_des_login_key_get(g_cfg.newcamd_key, (const uint8_t *)hash, (int)hlen, key16);
	  memcpy(cl->protocol.wire.newcamd.key1, key16, 8);
	  memcpy(cl->protocol.wire.newcamd.key2, key16 + 8, 8);
	}
	secure_zero(key16, sizeof(key16));

	cl->ecm.caid      = acc->caid;
	cl->identity.client_id = sid;
	/* MGcamd is identified by the NCD525 custom login header.  An account
	 * having multiple CAIDs is not evidence that the peer is MGcamd. */
	cl->protocol.wire.newcamd.is_mgcamd = (cl->protocol.wire.newcamd.is_mgcamd || (g_cfg.newcamd_mgclient != 0)) ? 1 : 0;
	tcmg_strlcpy(cl->protocol.name, cl->protocol.wire.newcamd.is_mgcamd ? "mgcamd" : "newcamd", sizeof(cl->protocol.name));
	tcmg_strlcpy(cl->identity.user,        acc->user,            CFGKEY_LEN);
	tcmg_strlcpy(cl->identity.client_name, cfg_client_name(sid), sizeof(cl->identity.client_name));
	account_mark_login(acc, ip);
	ban_record_ok(ip);

	if (cl->protocol.wire.newcamd.is_mgcamd)
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
	uint16_t caid = cl->auth.account ? cl->auth.account->caid : cl->ecm.caid;

	memset(resp, 0, sizeof(resp));
	resp[0] = MSG_CARD_DATA;
	resp[4] = (uint8_t)(caid >> 8);
	resp[5] = (uint8_t)(caid & 0xFF);
	nc_send(cl, resp, 26, sid, mid, pid);
	tcmg_log_dbg(D_NEWCAMD, "%s CARD_DATA user='%s' caid=%04X sid=%04X",
	             cl->identity.ip, cl->identity.user, caid, sid);

	if (cl->protocol.wire.newcamd.is_mgcamd && cl->auth.account)
	{
		tcmg_log_dbg(D_NEWCAMD, "%s [mgcamd] sending ADDCARD for %d caid(s)",
		             cl->identity.ip, cl->auth.account->ncaids + 1);
		nc_send_addcard(cl, caid, 0, mid);
		for (int i = 0; i < cl->auth.account->ncaids; i++)
			if (cl->auth.account->caids[i] != caid)
				nc_send_addcard(cl, cl->auth.account->caids[i], 0, mid);
	}
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

    if (!cl || !cl->auth.account) {
        ncd_ecm_nak(cl, cmd, sid, mid, pid);
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
        case ECM_ACCESS_EXPIRED:
            tcmg_log("%s ECM denied: account expired mid-session user='%s' expired=%ld",
                     cl->identity.ip, cl->identity.user, (long)cl->auth.account->expirationdate);
            break;
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
	tcmg_log_dbg(D_CONN, "%s new newcamd/mgcamd connection fd=%d tid=%u",
	             cl.identity.ip, cl.session.fd, cl.identity.thread_id);

	{
		int recv_timeout = g_cfg.sock_timeout;
		if (g_cfg.server_keepalive > 0 && g_cfg.server_keepalive < recv_timeout)
			recv_timeout = g_cfg.server_keepalive;
		nc_init(&cl, g_cfg.newcamd_key, recv_timeout);
	}

	int ka_misses = 0;
	while (g_running && !cl.session.kill_flag)
	{
		if (session_idle_expired(&cl, time(NULL)))
		{
			time_t idle = time(NULL) - (cl.session.last_activity ? cl.session.last_activity : cl.ecm.last_ecm_time);
			tcmg_log("%s idle timeout: %lds >= max_idle=%ds disconnecting user='%s'",
			         cl.identity.ip, (long)idle, cl.auth.account->max_idle, cl.identity.user);
			break;
		}

		dlen = nc_recv(&cl, data, &sid, &mid, &pid, &caid_hdr);
		if (dlen == NET_RECV_TIMEOUT)
		{
			if (cl.auth.account && g_cfg.server_keepalive > 0)
			{
				uint8_t ka[3] = { MSG_KEEPALIVE, 0, 0 };
				if (nc_send(&cl, ka, sizeof(ka), cl.ecm.last_srvid, 0, 0) < 0) break;
				ka_misses++;
				tcmg_log_dbg(D_NEWCAMD, "%s SERVER_KEEPALIVE user='%s' miss=%d/%d",
				             cl.identity.ip, cl.identity.user, ka_misses, g_cfg.server_keepalive_misses);
				if (ka_misses >= g_cfg.server_keepalive_misses) break;
				continue;
			}
			break;
		}
		if (dlen < 0)
		{
			if (cl.identity.user[0]) {
				S_ACCOUNT_STATS_SNAPSHOT stats;
				account_stats_snapshot(cl.auth.account, &stats);
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

		if (!cl.auth.account && cmd != MSG_CLIENT_LOGIN)
		{
			tcmg_log_dbg(D_NEWCAMD, "%s command 0x%02X rejected before authentication", cl.identity.ip, cmd);
			ncd_ecm_nak(&cl, cmd, sid, mid, pid);
			break;
		}
		if (cl.auth.account && cmd == MSG_CLIENT_LOGIN)
		{
			tcmg_log_dbg(D_NEWCAMD, "%s repeated login rejected for user='%s'", cl.identity.ip, cl.identity.user);
			ncd_ecm_nak(&cl, cmd, sid, mid, pid);
			break;
		}

		if      (cmd == MSG_CLIENT_LOGIN)
		{ if (!ncd_handle_login(&cl, data, dlen, sid, mid, pid)) break; }
		else if (cmd == MSG_CARD_DATA_REQ)
		{ ncd_handle_card(&cl, sid, mid, pid); }
		else if (cmd == MSG_KEEPALIVE)
		{
			tcmg_log_dbg(D_NEWCAMD, "%s KEEPALIVE user='%s'", cl.identity.ip, cl.identity.user);
			if (g_cfg.newcamd_keepalive)
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
