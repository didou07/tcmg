#include "stats/account_stats.h"
#define MODULE_LOG_PREFIX "cccam"
#include "../core/config_state.h"
#include "../core/runtime_state.h"
#include "../core/utils.h"
#include "../platform/platform.h"
#include "../security/failban.h"
#include "../log/log.h"
#include "../net/net.h"
#include "../crypto/crypto.h"
#include "reader/reader.h"
#include "account/account.h"
#include "account/account_state.h"
#include "ecm/ecm.h"
#include "session/session.h"
#include "cccam.h"
#include "server.h"

static S_PROTO_SERVER s_server;


static void cc_rc4_init(S_CC_CRYPT *b, const uint8_t *key, int klen)
{
    uint8_t j=0,tmp; int i;
    for(i=0;i<256;i++) b->keytable[i]=(uint8_t)i;
    for(i=0;i<256;i++){
        j+=key[i%klen]+b->keytable[i];
        tmp=b->keytable[i];b->keytable[i]=b->keytable[j];b->keytable[j]=tmp;}
    b->state=key[0];b->counter=0;b->sum=0;
}
static void cc_decrypt(S_CC_CRYPT *b, uint8_t *data, int len)
{
    uint8_t z,tmp; int i;
    for(i=0;i<len;i++){
        b->counter++;b->sum+=b->keytable[b->counter];
        tmp=b->keytable[b->counter];
        b->keytable[b->counter]=b->keytable[b->sum];b->keytable[b->sum]=tmp;
        z=data[i];
        data[i]=z^b->keytable[(b->keytable[b->counter]+b->keytable[b->sum])&0xFF]^b->state;
        z=data[i];b->state^=z;}
}
static void cc_encrypt(S_CC_CRYPT *b, uint8_t *data, int len)
{
    uint8_t z,tmp; int i;
    for(i=0;i<len;i++){
        b->counter++;b->sum+=b->keytable[b->counter];
        tmp=b->keytable[b->counter];
        b->keytable[b->counter]=b->keytable[b->sum];b->keytable[b->sum]=tmp;
        z=data[i];
        data[i]=z^b->keytable[(b->keytable[b->counter]+b->keytable[b->sum])&0xFF]^b->state;
        b->state^=z;}
}

static void cc_seed_xor(uint8_t *buf)
{
    static const uint8_t ccstr[6]={'C','C','c','a','m',0};
    uint8_t i;
    for(i=0;i<8;i++){buf[i+8]=(uint8_t)(i*buf[i]);if(i<=5)buf[i]^=ccstr[i];}
}

static void cc_derive_keys(S_CCCAM_CLIENT *cc, const uint8_t *seed)
{
    uint8_t xseed[16], hash[20], dec_seed[16], hash_buf[20];
    memcpy(xseed, seed, 16);
    cc_seed_xor(xseed);
    sha1_hash(xseed, 16, hash);
    cc_rc4_init(&cc->send_block, hash, 20);
    memcpy(dec_seed, xseed, 16);
    cc_decrypt(&cc->send_block, dec_seed, 16);
    cc_rc4_init(&cc->recv_block, dec_seed, 16);
    memcpy(hash_buf, hash, 20);
    cc_decrypt(&cc->recv_block, hash_buf, 20);
    secure_zero(xseed,    sizeof(xseed));
    secure_zero(hash,     sizeof(hash));
    secure_zero(hash_buf, sizeof(hash_buf));
    secure_zero(dec_seed, sizeof(dec_seed));
}

static int cc_send_msg(S_CCCAM_CLIENT *cc, uint8_t cmd,
                       const uint8_t *payload, uint16_t plen)
{
    uint8_t buf[CCCAM_MSG_MAX+4];
    if(plen>CCCAM_MSG_MAX) return -1;
    buf[0]=cc->g_flag;
    buf[1]=cmd;
    buf[2]=(uint8_t)(plen>>8);
    buf[3]=(uint8_t)(plen&0xFF);
    if(plen) memcpy(buf+4,payload,plen);
    cc_encrypt(&cc->send_block,buf,4+(int)plen);
    return net_send_all(cc->fd,buf,4+(int)plen);
}

static int cc_recv_msg(S_CCCAM_CLIENT *cc, uint8_t *seq_out, uint8_t *cmd,
                       uint8_t *buf, uint16_t *plen)
{
    uint8_t hdr[4]; uint16_t len;
    { int rr = net_recv_all(cc->fd,hdr,4);
      if(rr == NET_RECV_TIMEOUT) return NET_RECV_TIMEOUT;
      if(rr != 4) return -1; }
    cc_decrypt(&cc->recv_block,hdr,4);
    *seq_out=hdr[0];
    cc->g_flag=hdr[0];
    *cmd=hdr[1];
    len=((uint16_t)hdr[2]<<8)|hdr[3];
    if(len>CCCAM_MSG_MAX) return -1;
    *plen=len;
    if(len==0) return 0;
    { int rr = net_recv_all(cc->fd,buf,(int)len);
      if(rr == NET_RECV_TIMEOUT) return NET_RECV_TIMEOUT;
      if(rr != (int)len) return -1; }
    cc_decrypt(&cc->recv_block,buf,(int)len);
    return 0;
}

static void cc_cw_crypt(S_CCCAM_CLIENT *cc, uint8_t *cw, uint32_t card_id)
{
    uint8_t nod[8], n, tmp;
    int i, j;
    const uint8_t *nid = (cc->peer_node_id[0]||cc->peer_node_id[1]||cc->peer_node_id[2]||
                          cc->peer_node_id[3]||cc->peer_node_id[4]||cc->peer_node_id[5]||
                          cc->peer_node_id[6]||cc->peer_node_id[7])
                         ? cc->peer_node_id : cc->node_id;
    for(i=0;i<8;i++) nod[i]=nid[7-i];
    for(i=0;i<16;i++){
        j=i>>1;
        if(i&1)
            n=(i!=15)?((nod[j]>>4)|(nod[j+1]<<4))&0xFF:(nod[j]>>4)&0xFF;
        else
            n=nod[j];
        tmp=(uint8_t)(cw[i]^n);
        if(i&1) tmp=(uint8_t)(~tmp);
        cw[i]=(uint8_t)(((card_id>>(2*i))^tmp)&0xFF);
    }
}

static int cc_send_srv_data(S_CCCAM_CLIENT *cc)
{
    uint8_t buf[0x48];
    memset(buf,0,sizeof(buf));
    csprng(cc->node_id,8);
    memcpy(buf,cc->node_id,8);
    snprintf((char*)buf+8,32,"CCcam 2.3.0");
    snprintf((char*)buf+40,7,"3291");
    return cc_send_msg(cc,CCCAM_CMD_SRV_DATA,buf,sizeof(buf));
}

static void cc_send_new_card(S_CCCAM_CLIENT *cc, uint32_t card_id, uint16_t caid,
                              const uint32_t *provids, int nprov)
{
    uint8_t  buf[21 + 7 * 32];
    int      i, n;
    uint16_t total;

    n = (nprov < 32) ? nprov : 32;
    total = (uint16_t)(21 + 7 * n);
    memset(buf, 0, total);

    buf[0]  = (uint8_t)(card_id >> 24);
    buf[1]  = (uint8_t)(card_id >> 16);
    buf[2]  = (uint8_t)(card_id >>  8);
    buf[3]  = (uint8_t)(card_id & 0xFF);
    buf[8]  = (uint8_t)(caid >> 8);
    buf[9]  = (uint8_t)(caid & 0xFF);
    buf[20] = (uint8_t)n;

    for (i = 0; i < n; i++) {
        buf[21 + i*7]     = (uint8_t)(provids[i] >> 16);
        buf[21 + i*7 + 1] = (uint8_t)(provids[i] >>  8);
        buf[21 + i*7 + 2] = (uint8_t)(provids[i] & 0xFF);
    }
    cc_send_msg(cc, CCCAM_CMD_NEW_CARD, buf, total);
}

static void cc_send_cards(S_CCCAM_CLIENT *cc, const S_ACCOUNT *acc)
{
    uint32_t zero = 0;
    uint32_t card_id = 1;
    int      i, total = 0;

    if (acc->caid) {
        cc_send_new_card(cc, card_id++, acc->caid, &zero, 1);
        total++;
    }

    for (i = 0; i < acc->ncaids; i++)
        if (acc->caids[i]) {
            cc_send_new_card(cc, card_id++, acc->caids[i], &zero, 1);
            total++;
        }

    tcmg_log_dbg(D_CCCAM, "sent %d card(s) to user='%s'", total, acc->user);
}


/* CCCam transmits a fixed-width 20-byte username. Authenticate against that
 * exact wire field and verify the password proof using a copy of the receive
 * stream state. The accepted stream state is then installed for subsequent
 * encrypted messages. This also disambiguates accounts sharing the same first
 * 20 bytes, matching OSCam's authentication model. */
static S_ACCOUNT *cc_authenticate_account(S_CCCAM_CLIENT *cc,
                                          const uint8_t username[20],
                                          const uint8_t encrypted_cccam[6])
{
    if (!cc || !username || !encrypted_cccam) return NULL;

    const S_CC_CRYPT base = cc->recv_block;
    S_CC_CRYPT accepted_state;
    S_ACCOUNT *accepted = NULL;

    account_state_read_lock();
    for (S_ACCOUNT *acc = account_state_list_locked(); acc; acc = acc->next)
    {
        uint8_t acc_user[20];
        uint8_t pwd[CFGKEY_LEN];
        uint8_t check[6];
        memset(acc_user, 0, sizeof(acc_user));
        {
            size_t ulen = strlen(acc->user);
            if (ulen > sizeof(acc_user)) ulen = sizeof(acc_user);
            memcpy(acc_user, acc->user, ulen);
        }
        if (memcmp(username, acc_user, sizeof(acc_user)) != 0)
            continue;

        size_t pwlen = strlen(acc->pass);
        if (pwlen > sizeof(pwd))
            continue;
        memset(pwd, 0, sizeof(pwd));
        memcpy(pwd, acc->pass, pwlen);

        S_CC_CRYPT trial = base;
        cc_encrypt(&trial, pwd, (int)pwlen);
        memcpy(check, encrypted_cccam, sizeof(check));
        cc_decrypt(&trial, check, sizeof(check));

        bool match = memcmp(check, "CCcam\0", sizeof(check)) == 0;
        secure_zero(check, sizeof(check));
        secure_zero(pwd, sizeof(pwd));
        secure_zero(acc_user, sizeof(acc_user));
        if (!match)
            continue;

        account_retain(acc);
        accepted = acc;
        accepted_state = trial;
        break;
    }
    account_state_read_unlock();

    if (accepted)
        cc->recv_block = accepted_state;
    return accepted;
}

static void cc_handle_ecm(S_CCCAM_CLIENT *cc, S_CLIENT *cl,
                           uint8_t req_seq, const uint8_t *p, uint16_t plen)
{
    uint8_t resp[16] = {0};
    uint8_t cw[CW_LEN] = {0};
    uint16_t caid, sid;
    uint32_t provid, card_id;
    uint8_t ecm_len;
    T_ECM_ACCESS_STATUS access;
    S_ECM_RESULT result;

    (void)req_seq;
    if (!cl || !cl->auth.account || plen < 13) {
        if (cl) cc_send_msg(cc, CCCAM_CMD_ECM_NOK1, NULL, 0);
        return;
    }

    caid = ((uint16_t)p[0] << 8) | p[1];
    provid = ((uint32_t)p[2] << 24) | ((uint32_t)p[3] << 16) |
             ((uint32_t)p[4] << 8) | p[5];
    card_id = ((uint32_t)p[6] << 24) | ((uint32_t)p[7] << 16) |
              ((uint32_t)p[8] << 8) | p[9];
    sid = ((uint16_t)p[10] << 8) | p[11];
    ecm_len = p[12];

    if (ecm_len == 0 || plen < (uint16_t)(13 + ecm_len)) {
        tcmg_log_dbg(D_CCCAM, "%s ECM bad ecm_len=%u plen=%u",
                     cl->identity.ip, ecm_len, plen);
        cc_send_msg(cc, CCCAM_CMD_ECM_NOK1, NULL, 0);
        return;
    }

    access = ecm_access(cl, caid, sid, false, true, false);
    if (access != ECM_ACCESS_OK) {
        switch (access) {
        case ECM_ACCESS_CAID_DENIED:
            tcmg_log("%s ECM denied: caid=%04X not permitted for user='%s'",
                     cl->identity.ip, caid, cl->identity.user);
            break;
        case ECM_ACCESS_ANTISHARE:
            tcmg_log("%s ECM denied: anti-sharing limit triggered for user='%s' sid=%04X",
                     cl->identity.ip, cl->identity.user, sid);
            break;
        case ECM_ACCESS_DISABLED:
            tcmg_log("%s ECM denied: account disabled mid-session user='%s'",
                     cl->identity.ip, cl->identity.user);
            break;
        case ECM_ACCESS_EXPIRED:
            tcmg_log("%s ECM denied: account expired mid-session user='%s' expired=%ld",
                     cl->identity.ip, cl->identity.user, (long)cl->auth.account->expirationdate);
            break;
        default:
            break;
        }
        cc_send_msg(cc, CCCAM_CMD_ECM_NOK1, NULL, 0);
        return;
    }

    tcmg_log_dbg(D_CCCAM,
                 "%s ECM request user='%s' caid=%04X sid=%04X provid=%06X card_id=%08X ecm_len=%u channel='%s'",
                 cl->identity.ip, cl->identity.user, caid, sid, provid, card_id, ecm_len,
                 cl->ecm.last_channel[0] ? cl->ecm.last_channel : "unknown");

    if (ecm_process(cl, caid, sid, provid, p + 13, ecm_len, cw, &result) < 0) {
        cc_send_msg(cc, CCCAM_CMD_ECM_NOK1, NULL, 0);
        secure_zero(cw, sizeof(cw));
        return;
    }

    memcpy(resp, cw, CW_LEN);
    cc_cw_crypt(cc, resp, card_id);
    cc_send_msg(cc, CCCAM_CMD_ECM_REQ, resp, CW_LEN);
    cc_encrypt(&cc->send_block, resp, CW_LEN);
    tcmg_dump_dbg(D_CCCAM, cc->peer_node_id, 8,
                  "%s cw peer node card_id=%08X node", cl->identity.ip, card_id);
    tcmg_dump_dbg(D_CCCAM, cw, CW_LEN,
                  "%s CW sent to user='%s' caid=%04X sid=%04X",
                  cl->identity.ip, cl->identity.user, caid, sid);
    tcmg_log_dbg(D_CCCAM,
                 "%s ECM result=FOUND user='%s' caid=%04X sid=%04X time=%ldms cache=%d",
                 cl->identity.ip, cl->identity.user, caid, sid, (long)result.elapsed_ms,
                 result.cache_hit ? 1 : 0);
    secure_zero(cw, sizeof(cw));
    (void)result;
}

void *handle_cccam_client(void *arg)
{
    S_CONN_ARGS    *args=(S_CONN_ARGS*)arg;
    S_CCCAM_CLIENT  cc;
    S_CLIENT        cl;
    uint8_t         seed[CCCAM_SEED_LEN];
    uint8_t         cli_hash[CCCAM_HASH_LEN];
    uint8_t         username[20];
    uint8_t         ccstr_recv[6];
    uint8_t         ack[20];
    S_ACCOUNT      *acc;
    char            user[CFGKEY_LEN];
    uint8_t         cmd,req_seq;
    uint8_t         payload[CCCAM_MSG_MAX];
    uint16_t        plen;

    memset(&cc, 0, sizeof(cc));
    {
        T_SESSION_INPUT input = { args->fd, args->ip };
        session_init(&cl, &input, "cccam");
    }
    cc.fd = cl.session.fd;
        free(args);
    tcmg_log_dbg(D_CONN,"%s new connection fd=%d tid=%u",
                 cl.identity.ip, cl.session.fd, cl.identity.thread_id);

    {
        int recv_timeout = g_cfg.sock_timeout;
        if (g_cfg.server_keepalive > 0 && g_cfg.server_keepalive < recv_timeout)
            recv_timeout = g_cfg.server_keepalive;
        net_set_timeout(cc.fd, recv_timeout);
    }
    net_tune_socket(cc.fd);

    if(ban_is_banned(cl.identity.ip)){
        tcmg_log("%s LOGIN failed: IP is banned", cl.identity.ip);
        goto cleanup;
    }

    csprng(seed,CCCAM_SEED_LEN);
    tcmg_log_dbg(D_CCCAM, "%s sending %d-byte seed", cl.identity.ip, CCCAM_SEED_LEN);
    if(net_send_all(cc.fd,seed,CCCAM_SEED_LEN)!=CCCAM_SEED_LEN) goto cleanup;

    cc_derive_keys(&cc,seed);
    secure_zero(seed,sizeof(seed));
    tcmg_log_dbg(D_CCCAM, "%s session keys derived", cl.identity.ip);

    if(net_recv_all(cc.fd,cli_hash,CCCAM_HASH_LEN)!=CCCAM_HASH_LEN) {
        tcmg_log_dbg(D_CCCAM, "%s failed to receive client hash", cl.identity.ip);
        goto cleanup;
    }
    cc_decrypt(&cc.recv_block,cli_hash,CCCAM_HASH_LEN);
    secure_zero(cli_hash,sizeof(cli_hash));

    if(net_recv_all(cc.fd,username,20)!=20) {
        tcmg_log_dbg(D_CCCAM, "%s failed to receive username", cl.identity.ip);
        goto cleanup;
    }
    cc_decrypt(&cc.recv_block,username,20);
    memset(user,0,sizeof(user));
    memcpy(user, username, sizeof(username));
    user[sizeof(user) - 1] = '\0';

    tcmg_log_dbg(D_CCCAM, "%s LOGIN attempt user='%s'", cl.identity.ip, user);

    if(net_recv_all(cc.fd,ccstr_recv,6)!=6) {
        tcmg_log_dbg(D_CCCAM, "%s failed to receive CCcam password proof user='%s'",
                     cl.identity.ip, user);
        goto cleanup;
    }

    acc=cc_authenticate_account(&cc,username,ccstr_recv);
    cl.auth.account=acc;
    secure_zero(ccstr_recv,sizeof(ccstr_recv));
    secure_zero(username,sizeof(username));

    if(!acc){
        tcmg_log("%s LOGIN failed: unknown user or invalid password user='%s'", cl.identity.ip, user);
        ban_record_fail(cl.identity.ip); goto cleanup;
    }

    {
        T_ACCOUNT_STATUS status = account_validate(acc, cl.identity.ip);
        if (status != ACCOUNT_OK) {
            if (status == ACCOUNT_DISABLED)
                tcmg_log("%s LOGIN failed: account disabled user='%s'", cl.identity.ip, acc->user);
            else if (status == ACCOUNT_EXPIRED)
                tcmg_log("%s LOGIN failed: account expired user='%s' expired=%ld",
                         cl.identity.ip, acc->user, (long)acc->expirationdate);
            else if (status == ACCOUNT_IP_DENIED)
                tcmg_log("%s LOGIN failed: IP not whitelisted user='%s'", cl.identity.ip, acc->user);
            goto cleanup;
        }
    }

    memset(ack,0,sizeof(ack));
    memcpy(ack,"CCcam",5);
    cc_encrypt(&cc.send_block,ack,20);
    if(net_send_all(cc.fd,ack,20)!=20) goto cleanup;
    secure_zero(ack,sizeof(ack));

    if (account_session_open(&cl, acc) < 0) {
        tcmg_log("%s LOGIN failed: max_connections=%d reached for user='%s' active=%d",
                 cl.identity.ip, acc->max_connections, acc->user, (int)acc->active);
        goto cleanup;
    }

    tcmg_strlcpy(cl.identity.user,acc->user,CFGKEY_LEN);
    cl.ecm.caid=acc->caid;
    cl.session.last_activity=time(NULL);

    account_mark_login(acc, cl.identity.ip);
    ban_record_ok(cl.identity.ip);

    {
        int card_count = acc->ncaids + (acc->caid ? 1 : 0);
        tcmg_log("%s LOGIN ok user='%s' cards=%d max_conn=%d",
                 cl.identity.ip, acc->user, card_count, acc->max_connections);
    }

    if (cc_send_msg(&cc,CCCAM_CMD_CLI_DATA,NULL,0) < 0) goto done;
    tcmg_log_dbg(D_CCCAM, "%s CLI_DATA ack sent to user='%s'", cl.identity.ip, acc->user);
    if (cc_send_srv_data(&cc) < 0) goto done;
    tcmg_log_dbg(D_CCCAM, "%s SRV_DATA sent to user='%s'", cl.identity.ip, acc->user);

    /* OSCam waits for the client's post-SRV_DATA CLI_DATA before publishing
     * the card list. This makes the handshake deterministic and prevents card
     * frames from racing the client's server-data parser. */
    {
        int ready = 0;
        for (int n = 0; n < 4 && !ready; n++) {
            uint8_t rcmd, rseq; uint16_t rplen;
            int rr = cc_recv_msg(&cc, &rseq, &rcmd, payload, &rplen);
            if (rr == NET_RECV_TIMEOUT) {
                if (g_cfg.server_keepalive > 0) {
                    if (cc_send_msg(&cc, CCCAM_CMD_KEEPALIVE, NULL, 0) < 0) goto done;
                    continue;
                }
                goto done;
            }
            if (rr < 0) goto done;
            if (rcmd == CCCAM_CMD_CLI_DATA) {
                if (rplen >= 28) memcpy(cc.peer_node_id, payload + 20, 8);
                ready = 1;
            } else if (rcmd == CCCAM_CMD_KEEPALIVE) {
                if (cc_send_msg(&cc, CCCAM_CMD_KEEPALIVE, NULL, 0) < 0) goto done;
            }
        }
        if (!ready) goto done;
    }
    cc_send_cards(&cc,acc);

    int ka_misses = 0;
    while(g_running&&!cl.session.kill_flag){
        if (session_idle_expired(&cl, time(NULL))) {
            time_t idle=time(NULL)-(cl.session.last_activity ? cl.session.last_activity : cl.ecm.last_ecm_time);
            tcmg_log("%s idle timeout %lds >= max_idle=%ds disconnecting user='%s'",
                     cl.identity.ip, (long)idle, cl.auth.account->max_idle, cl.identity.user);
            break;
        }

        int rr = cc_recv_msg(&cc,&req_seq,&cmd,payload,&plen);
        if(rr == NET_RECV_TIMEOUT){
            if(g_cfg.server_keepalive > 0){
                if(cc_send_msg(&cc,CCCAM_CMD_KEEPALIVE,NULL,0)<0) break;
                ka_misses++;
                tcmg_log_dbg(D_CCCAM, "%s SERVER_KEEPALIVE user='%s' miss=%d/%d",
                             cl.identity.ip, cl.identity.user, ka_misses, g_cfg.server_keepalive_misses);
                if(ka_misses >= g_cfg.server_keepalive_misses) break;
                continue;
            }
            break;
        }
        if(rr<0){
            if (cl.identity.user[0]) {
                S_ACCOUNT_STATS_SNAPSHOT stats;
                account_stats_snapshot(cl.auth.account, &stats);
                tcmg_log("%s disconnected user='%s' ecm_total=%llu cw_found=%lld",
                         cl.identity.ip, cl.identity.user,
                         (unsigned long long)stats.ecm_total,
                         (long long)stats.cw_found);
            }
            else
                tcmg_log_dbg(D_CONN, "%s disconnected (no user)", cl.identity.ip);
            break;
        }

        ka_misses = 0;
        cl.session.last_activity = time(NULL);
        tcmg_log_dbg(D_CCCAM, "%s recv cmd=0x%02X plen=%u seq=%u",
                     cl.identity.ip, cmd, plen, req_seq);

        if(cmd==CCCAM_CMD_ECM_REQ){
            cc_handle_ecm(&cc,&cl,req_seq,payload,plen);
        } else if(cmd==CCCAM_CMD_KEEPALIVE){
            tcmg_log_dbg(D_CCCAM, "%s KEEPALIVE user='%s'", cl.identity.ip, cl.identity.user);
            cc_send_msg(&cc,CCCAM_CMD_KEEPALIVE,NULL,0);
        } else if(cmd==CCCAM_CMD_CLI_DATA){
            tcmg_log_dbg(D_CCCAM, "%s CLI_DATA user='%s' plen=%u", cl.identity.ip, cl.identity.user, plen);
            if(plen>=28) memcpy(cc.peer_node_id, payload+20, 8);
            cc_send_msg(&cc,CCCAM_CMD_CLI_DATA,NULL,0);
        } else if(cmd==CCCAM_CMD_EMM_REQ){
            tcmg_log_dbg(D_CCCAM, "%s EMM_REQ user='%s' plen=%u (ignored)", cl.identity.ip, cl.identity.user, plen);
            cc_send_msg(&cc,CCCAM_CMD_EMM_REQ,NULL,0);
        } else if(cmd==0x0C||cmd==0x0D||cmd==0x0E){
            tcmg_log_dbg(D_CCCAM, "%s cmd=0x%02X user='%s' plen=%u (echo)", cl.identity.ip, cmd, cl.identity.user, plen);
            cc_send_msg(&cc,cmd,NULL,0);
        } else {
            tcmg_log_dbg(D_CCCAM, "%s unknown cmd=0x%02X plen=%u -- ignored",
                         cl.identity.ip, cmd, plen);
        }
    }

done:
cleanup:
    tcmg_log_dbg(D_CONN, "%s connection closed fd=%d tid=%u", cl.identity.ip, cl.session.fd, cl.identity.thread_id);
    session_cleanup(&cl);
    return NULL;
}

int32_t cccam_start(void)
{
    return proto_server_start(&s_server, "cccam", g_cfg.cccam_port,
                               g_cfg.cccam_bindaddr,
                               handle_cccam_client);
}

void cccam_stop(void)
{
    proto_server_stop(&s_server);
}
