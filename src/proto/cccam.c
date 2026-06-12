#define MODULE_LOG_PREFIX "cccam"
#include "../../globals.h"
#include "cccam.h"
#include "../client/client.h"

static _Atomic int32_t   s_cccam_running = 0;
static pthread_t         s_cccam_thread;
static int               s_cccam_srv_fd  = -1;


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
    uint8_t xseed[16],hash[20],dec_seed[16];
    memcpy(xseed,seed,16);
    cc_seed_xor(xseed);
    sha1_hash(xseed,16,hash);
    cc_rc4_init(&cc->recv_block,hash,20);
    memcpy(dec_seed,xseed,16);
    cc_decrypt(&cc->recv_block,dec_seed,16);
    cc_rc4_init(&cc->send_block,dec_seed,16);
    secure_zero(xseed,sizeof(xseed));
    secure_zero(hash,sizeof(hash));
    secure_zero(dec_seed,sizeof(dec_seed));
}

static int cc_send_msg(S_CCCAM_CLIENT *cc, uint8_t cmd,
                       const uint8_t *payload, uint16_t plen)
{
    uint8_t buf[CCCAM_MSG_MAX+4];
    if(plen>CCCAM_MSG_MAX) return -1;
    buf[0]=cc->seq++;
    buf[1]=cmd;
    buf[2]=(uint8_t)(plen>>8);
    buf[3]=(uint8_t)(plen&0xFF);
    if(plen) memcpy(buf+4,payload,plen);
    cc_encrypt(&cc->recv_block,buf,4+(int)plen);
    return net_send_all(cc->fd,buf,4+(int)plen);
}

static int cc_recv_msg(S_CCCAM_CLIENT *cc, uint8_t *seq_out, uint8_t *cmd,
                       uint8_t *buf, uint16_t *plen)
{
    uint8_t hdr[4]; uint16_t len;
    if(net_recv_all(cc->fd,hdr,4)!=4) return -1;
    cc_decrypt(&cc->send_block,hdr,4);
    *seq_out=hdr[0]; *cmd=hdr[1];
    len=((uint16_t)hdr[2]<<8)|hdr[3];
    if(len>CCCAM_MSG_MAX) return -1;
    *plen=len;
    if(len==0) return 0;
    if(net_recv_all(cc->fd,buf,(int)len)!=(int)len) return -1;
    cc_decrypt(&cc->send_block,buf,(int)len);
    return 0;
}

static void cc_cw_crypt(S_CCCAM_CLIENT *cc, uint8_t *cw, uint32_t card_id)
{
    uint8_t nod[8], n, tmp;
    int i, j;
    for(i=0;i<8;i++) nod[i]=cc->node_id[7-i];
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

static void cc_send_srv_data(S_CCCAM_CLIENT *cc)
{
    uint8_t buf[0x4c];
    memset(buf,0,sizeof(buf));
    csprng(cc->node_id,8);
    memcpy(buf,cc->node_id,8);
    snprintf((char*)buf+8,32,"CCcam 2.3.0");
    snprintf((char*)buf+40,7,"3291");
    cc_send_msg(cc,CCCAM_CMD_SRV_DATA,buf,sizeof(buf));
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

static void cc_handle_ecm(S_CCCAM_CLIENT *cc, S_CLIENT *cl,
                           uint8_t req_seq, const uint8_t *p, uint16_t plen)
{
    uint8_t  resp[16], cw[CW_LEN], ecm_md5[16];
    uint16_t caid, sid;
    uint32_t provid, card_id;
    uint8_t  ecm_len;
    int32_t  res;
    bool     cache_hit;
    S_ECM_CTX ctx;
    int64_t   t0_ms;
    long      ms = 0;

    (void)req_seq;

    if(plen < 13){
        tcmg_log_dbg(D_CCCAM, "%s [cccam] ECM packet too short plen=%u expected>=13",
                     cl->ip, plen);
        cc_send_msg(cc,CCCAM_CMD_ECM_NOK1,NULL,0); return;
    }
    caid   =((uint16_t)p[0]<<8)|p[1];
    provid =((uint32_t)p[2]<<24)|((uint32_t)p[3]<<16)|((uint32_t)p[4]<<8)|p[5];
    card_id=((uint32_t)p[6]<<24)|((uint32_t)p[7]<<16)|((uint32_t)p[8]<<8)|p[9];
    sid    =((uint16_t)p[10]<<8)|p[11];
    ecm_len= p[12];

    if(ecm_len==0||plen<(uint16_t)(13+ecm_len)){
        tcmg_log_dbg(D_CCCAM, "%s [cccam] ECM bad ecm_len=%u plen=%u",
                     cl->ip, ecm_len, plen);
        cc_send_msg(cc,CCCAM_CMD_ECM_NOK1,NULL,0); return;
    }

    cl->last_ecm_time=time(NULL);
    cl->last_caid=caid; cl->last_srvid=sid;
    srvid_lookup_copy(caid,sid,cl->last_channel,sizeof(cl->last_channel));

    tcmg_log_dbg(D_CCCAM, "%s [cccam] ECM request user='%s' caid=%04X sid=%04X provid=%06X card_id=%08X ecm_len=%u channel='%s'",
                 cl->ip, cl->user, caid, sid, provid, card_id, ecm_len,
                 cl->last_channel[0] ? cl->last_channel : "unknown");

    if (D_CCCAM & g_dblevel)
        log_ecm_raw(caid, sid, p+13, ecm_len);

    tcmg_strlcpy(ctx.user,cl->user,CFGKEY_LEN);
    tcmg_strlcpy(ctx.ip,cl->ip,MAXIPLEN);
    ctx.fd=cl->fd; ctx.caid=caid;
    ctx.thread_id=cl->thread_id; ctx.account=cl->account;

    crypt_md5_hash(p+13, ecm_len, ecm_md5);
    memset(cw,0,CW_LEN);
    cache_hit = cw_cache_lookup(ecm_md5, cw);
    if(cache_hit){
        res = EMU_OK;
        ms  = 0;
        tcmg_log_dbg(D_CCCAM, "%s [cccam] ECM cache HIT user='%s' caid=%04X sid=%04X",
                     cl->ip, cl->user, caid, sid);
    } else {
        t0_ms = tcmg_mono_ms();
        res=emu_process(caid,sid,p+13,ecm_len,cw,&ctx);
        ms = (long)tcmg_elapsed_ms(t0_ms);
        if(res==EMU_OK) cw_cache_store(ecm_md5, cw);
    }

    if(res==EMU_OK){
        memcpy(resp, cw, 16);
        cc_cw_crypt(cc, resp, card_id);
        cc_send_msg(cc,CCCAM_CMD_ECM_REQ,resp,16);
        cc_encrypt(&cc->recv_block,resp,16);
        tcmg_dump_dbg(D_CCCAM, cw, CW_LEN,
                      "%s [cccam] CW sent to user='%s' caid=%04X sid=%04X",
                      cl->ip, cl->user, caid, sid);

        pthread_mutex_lock(&cl->account->stat_mtx);
        cl->account->cw_found++; cl->account->ecm_total++;
        cl->account->cw_time_total_ms += ms;
        if (cl->account->cw_time_min_ms == 0 || ms < cl->account->cw_time_min_ms)
            cl->account->cw_time_min_ms = ms;
        if (ms > cl->account->cw_time_max_ms)
            cl->account->cw_time_max_ms = ms;
        pthread_mutex_unlock(&cl->account->stat_mtx);

        tcmg_log_dbg(D_CCCAM, "%s [cccam] ECM result=FOUND user='%s' caid=%04X sid=%04X time=%ldms",
                     cl->ip, cl->user, caid, sid, ms);
    } else {
        cc_send_msg(cc,CCCAM_CMD_ECM_NOK1,NULL,0);

        pthread_mutex_lock(&cl->account->stat_mtx);
        cl->account->cw_not++; cl->account->ecm_total++;
        pthread_mutex_unlock(&cl->account->stat_mtx);

        tcmg_log_dbg(D_CCCAM, "%s [cccam] ECM result=NOT_FOUND user='%s' caid=%04X sid=%04X emu_rc=%d time=%ldms",
                     cl->ip, cl->user, caid, sid, res, ms);
    }

    log_cw_result(caid, sid, ecm_len, cw, res == EMU_OK, cache_hit, (int32_t)ms, cl->user);
    secure_zero(cw,sizeof(cw));
    (void)provid;
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
    uint8_t         pwd_enc[256];
    S_ACCOUNT      *acc;
    char            user[CFGKEY_LEN];
    uint8_t         cmd,req_seq;
    uint8_t         payload[CCCAM_MSG_MAX];
    uint16_t        plen;

    memset(&cc,0,sizeof(cc)); memset(&cl,0,sizeof(cl));
    cc.fd=args->fd; cl.fd=args->fd;
    cl.thread_id=(uint32_t)(uintptr_t)pthread_self();
    cl.connect_time=time(NULL); cl.last_ecm_time=time(NULL);
    cl.is_mgcamd=0;
    tcmg_strlcpy(cl.proto, "cccam", sizeof(cl.proto));
    tcmg_strlcpy(cl.ip,args->ip,MAXIPLEN);
    free(args);

    log_set_type(LOG_TYPE_CLIENT);
    client_register(&cl);
    tcmg_log_dbg(D_CONN,"%s [cccam] new connection fd=%d tid=%u",
                 cl.ip, cl.fd, cl.thread_id);

    net_set_timeout(cc.fd,g_cfg.sock_timeout);
    net_tune_socket(cc.fd);

    if(ban_is_banned(cl.ip)){
        tcmg_log("%s [cccam] LOGIN failed: IP is banned", cl.ip);
        goto cleanup;
    }

    csprng(seed,CCCAM_SEED_LEN);
    tcmg_log_dbg(D_CCCAM, "%s [cccam] sending %d-byte seed", cl.ip, CCCAM_SEED_LEN);
    if(net_send_all(cc.fd,seed,CCCAM_SEED_LEN)!=CCCAM_SEED_LEN) goto cleanup;

    cc_derive_keys(&cc,seed);
    secure_zero(seed,sizeof(seed));
    tcmg_log_dbg(D_CCCAM, "%s [cccam] session keys derived", cl.ip);

    if(net_recv_all(cc.fd,cli_hash,CCCAM_HASH_LEN)!=CCCAM_HASH_LEN) {
        tcmg_log_dbg(D_CCCAM, "%s [cccam] failed to receive client hash", cl.ip);
        goto cleanup;
    }
    cc_decrypt(&cc.send_block,cli_hash,CCCAM_HASH_LEN);
    cc_encrypt(&cc.send_block,cli_hash,CCCAM_HASH_LEN);
    secure_zero(cli_hash,sizeof(cli_hash));

    if(net_recv_all(cc.fd,username,20)!=20) {
        tcmg_log_dbg(D_CCCAM, "%s [cccam] failed to receive username", cl.ip);
        goto cleanup;
    }
    cc_decrypt(&cc.send_block,username,20);
    username[19]='\0';
    memset(user,0,sizeof(user));
    tcmg_strlcpy(user,(char*)username,sizeof(user));
    secure_zero(username,sizeof(username));

    tcmg_log_dbg(D_CCCAM, "%s [cccam] LOGIN attempt user='%s'", cl.ip, user);

    pthread_rwlock_rdlock(&g_cfg.acc_lock);
    acc=cfg_find_account(user);
    pthread_rwlock_unlock(&g_cfg.acc_lock);

    if(!acc){
        tcmg_log("%s [cccam] LOGIN failed: unknown user '%s'", cl.ip, user);
        ban_record_fail(cl.ip); goto cleanup;
    }
    if(!acc->enabled){
        tcmg_log("%s [cccam] LOGIN failed: account disabled user='%s'", cl.ip, user);
        goto cleanup;
    }

    {
        size_t pwlen=strlen(acc->pass);
        if(pwlen>0&&pwlen<=255){
            memcpy(pwd_enc,acc->pass,pwlen);
            cc_encrypt(&cc.send_block,pwd_enc,(int)pwlen);
            secure_zero(pwd_enc,pwlen);
        }
    }

    if(net_recv_all(cc.fd,ccstr_recv,6)!=6) {
        tcmg_log_dbg(D_CCCAM, "%s [cccam] failed to receive CCcam string user='%s'",
                     cl.ip, user);
        goto cleanup;
    }
    cc_decrypt(&cc.send_block,ccstr_recv,6);

    if(memcmp(ccstr_recv,"CCcam",5)!=0){
        tcmg_log("%s [cccam] LOGIN failed: wrong password for user='%s'", cl.ip, user);
        ban_record_fail(cl.ip); goto cleanup;
    }
    secure_zero(ccstr_recv,sizeof(ccstr_recv));

    memset(ack,0,sizeof(ack));
    memcpy(ack,"CCcam",5);
    cc_encrypt(&cc.recv_block,ack,20);
    if(net_send_all(cc.fd,ack,20)!=20) goto cleanup;
    secure_zero(ack,sizeof(ack));

    if(acc->max_connections>0){
        pthread_rwlock_wrlock(&g_cfg.acc_lock);
        bool over=((int32_t)acc->active>=acc->max_connections);
        if(!over) atomic_fetch_add(&acc->active,1);
        pthread_rwlock_unlock(&g_cfg.acc_lock);
        if(over){
            tcmg_log("%s [cccam] LOGIN failed: max_connections=%d reached for user='%s' active=%d",
                     cl.ip, acc->max_connections, acc->user, (int)acc->active);
            goto cleanup;
        }
    } else {
        atomic_fetch_add(&acc->active,1);
    }

    tcmg_strlcpy(cl.user,acc->user,CFGKEY_LEN);
    cl.account=acc; cl.caid=acc->caid;

    log_set_user(acc->user);
    atomic_store(&acc->last_seen, time(NULL));
    if(!acc->first_login) acc->first_login=time(NULL);
    ban_record_ok(cl.ip);

    {
        int card_count = acc->ncaids + (acc->caid ? 1 : 0);
        tcmg_log("%s [cccam] LOGIN ok user='%s' cards=%d max_conn=%d",
                 cl.ip, acc->user, card_count, acc->max_connections);
    }

    cc_send_msg(&cc,CCCAM_CMD_CLI_DATA,NULL,0);
    tcmg_log_dbg(D_CCCAM, "%s [cccam] CLI_DATA ack sent to user='%s'", cl.ip, acc->user);
    cc_send_srv_data(&cc);
    tcmg_log_dbg(D_CCCAM, "%s [cccam] SRV_DATA sent to user='%s'", cl.ip, acc->user);
    cc_send_cards(&cc,acc);

    while(g_running&&!cl.kill_flag){
        if(acc->max_idle>0){
            time_t idle=time(NULL)-cl.last_ecm_time;
            if(idle>=acc->max_idle){
                tcmg_log("%s [cccam] idle timeout %lds >= max_idle=%ds disconnecting user='%s'",
                         cl.ip, (long)idle, acc->max_idle, cl.user);
                break;
            }
        }

        if(cc_recv_msg(&cc,&req_seq,&cmd,payload,&plen)<0){
            if (cl.user[0])
                tcmg_log("%s [cccam] disconnected user='%s' ecm_total=%llu cw_found=%lld",
                         cl.ip, cl.user,
                         cl.account ? (unsigned long long)cl.account->ecm_total : 0ULL,
                         cl.account ? (long long)cl.account->cw_found : 0LL);
            else
                tcmg_log_dbg(D_CONN, "%s [cccam] disconnected (no user)", cl.ip);
            break;
        }

        tcmg_log_dbg(D_CCCAM, "%s [cccam] recv cmd=0x%02X plen=%u seq=%u",
                     cl.ip, cmd, plen, req_seq);

        if(cmd==CCCAM_CMD_ECM_REQ){
            cc_handle_ecm(&cc,&cl,req_seq,payload,plen);
        } else if(cmd==CCCAM_CMD_KEEPALIVE){
            tcmg_log_dbg(D_CCCAM, "%s [cccam] KEEPALIVE user='%s'", cl.ip, cl.user);
            cc_send_msg(&cc,CCCAM_CMD_KEEPALIVE,NULL,0);
        } else if(cmd==CCCAM_CMD_CLI_DATA){
            tcmg_log_dbg(D_CCCAM, "%s [cccam] CLI_DATA user='%s' plen=%u", cl.ip, cl.user, plen);
            cc_send_msg(&cc,CCCAM_CMD_CLI_DATA,NULL,0);
        } else if(cmd==CCCAM_CMD_EMM_REQ){
            tcmg_log_dbg(D_CCCAM, "%s [cccam] EMM_REQ user='%s' plen=%u (ignored)", cl.ip, cl.user, plen);
            cc_send_msg(&cc,CCCAM_CMD_EMM_REQ,NULL,0);
        } else if(cmd==0x0C||cmd==0x0D||cmd==0x0E){
            tcmg_log_dbg(D_CCCAM, "%s [cccam] cmd=0x%02X user='%s' plen=%u (echo)", cl.ip, cmd, cl.user, plen);
            cc_send_msg(&cc,cmd,NULL,0);
        } else {
            tcmg_log_dbg(D_CCCAM, "%s [cccam] unknown cmd=0x%02X plen=%u -- ignored",
                         cl.ip, cmd, plen);
        }
    }

    atomic_fetch_sub(&acc->active,1);

cleanup:
    client_unregister(&cl);
    tcmg_log_dbg(D_CONN, "%s [cccam] connection closed fd=%d tid=%u", cl.ip, cl.fd, cl.thread_id);
    close(cc.fd);
    atomic_fetch_sub(&g_active_conns,1);
    return NULL;
}

static void *cccam_listen_thread(void *arg)
{
    struct sockaddr_in ca; socklen_t clen;
    pthread_attr_t attr;
    (void)arg;

    log_set_type(LOG_TYPE_CLIENT);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&attr,256*1024);

    while(s_cccam_running&&g_running){
        fd_set rfds; FD_ZERO(&rfds); FD_SET(s_cccam_srv_fd,&rfds);
        struct timeval tv={1,0};
        if(select(s_cccam_srv_fd+1,&rfds,NULL,NULL,&tv)<=0) continue;

        clen=sizeof(ca);
        int cfd=(int)accept(s_cccam_srv_fd,(struct sockaddr*)&ca,&clen);
        if(cfd<0){
            if(s_cccam_running)
                tcmg_log_dbg(D_CONN, "[cccam] accept() failed errno=%d (%s)",
                             errno, strerror(errno));
            continue;
        }

        int active = atomic_fetch_add(&g_active_conns,1);
        if(active>=MAX_CONNS){
            atomic_fetch_sub(&g_active_conns,1); close(cfd);
            tcmg_log("[cccam] MAX_CONNS=%d reached -- connection rejected active=%d",
                     MAX_CONNS, active);
            continue;
        }

        S_CONN_ARGS *a=(S_CONN_ARGS*)malloc(sizeof(S_CONN_ARGS));
        if(!a){
            atomic_fetch_sub(&g_active_conns,1); close(cfd);
            tcmg_log("[cccam] out of memory -- connection rejected active=%d", active);
            continue;
        }
        a->fd=cfd;
        inet_ntop(AF_INET,&ca.sin_addr,a->ip,MAXIPLEN);
        a->ip[MAXIPLEN-1]='\0';

        tcmg_log_dbg(D_CONN, "%s [cccam] accepted connection fd=%d active=%d",
                     a->ip, cfd, active+1);

        pthread_t tid;
        if(pthread_create(&tid,&attr,handle_cccam_client,a)!=0){
            tcmg_log("[cccam] pthread_create failed errno=%d (%s)", errno, strerror(errno));
            atomic_fetch_sub(&g_active_conns,1); close(cfd); free(a);
        }
    }
    pthread_attr_destroy(&attr);
    tcmg_log_dbg(D_CONN, "%s", "[cccam] accept thread exiting");
    return NULL;
}

int32_t cccam_start(void)
{
    struct sockaddr_in sa; int opt=1;
    if(!g_cfg.cccam_port) {
        tcmg_log_dbg(D_CCCAM, "%s", "disabled (port=0)");
        return -1;
    }

    s_cccam_srv_fd=(int)socket(AF_INET,SOCK_STREAM,0);
    if(s_cccam_srv_fd<0){
        tcmg_log("[cccam] socket() failed errno=%d (%s)", errno, strerror(errno));
        return -1;
    }

    setsockopt(s_cccam_srv_fd,SOL_SOCKET,SO_REUSEADDR,SO_CAST(&opt),sizeof(opt));
    memset(&sa,0,sizeof(sa));
    sa.sin_family=AF_INET; sa.sin_addr.s_addr=INADDR_ANY;
    sa.sin_port=htons((uint16_t)g_cfg.cccam_port);

    if(bind(s_cccam_srv_fd,(struct sockaddr*)&sa,sizeof(sa))<0){
        tcmg_log("[cccam] bind() failed port=%d errno=%d (%s)",
                 g_cfg.cccam_port, errno, strerror(errno));
        close(s_cccam_srv_fd);s_cccam_srv_fd=-1;return -1;
    }
    if(listen(s_cccam_srv_fd,64)<0){
        tcmg_log("[cccam] listen() failed errno=%d (%s)", errno, strerror(errno));
        close(s_cccam_srv_fd);s_cccam_srv_fd=-1;return -1;
    }

    s_cccam_running=1;
    if(pthread_create(&s_cccam_thread,NULL,cccam_listen_thread,NULL)!=0){
        tcmg_log("[cccam] failed to start listener errno=%d (%s)", errno, strerror(errno));
        s_cccam_running=0;close(s_cccam_srv_fd);s_cccam_srv_fd=-1;return -1;
    }
    tcmg_log("listening on port=%d", g_cfg.cccam_port);
    return 0;
}

void cccam_stop(void)
{
    if(!s_cccam_running) return;
    tcmg_log_dbg(D_CCCAM, "%s", "[cccam] stopping...");
    s_cccam_running=0;
    if(s_cccam_srv_fd>=0){close(s_cccam_srv_fd);s_cccam_srv_fd=-1;}
    pthread_join(s_cccam_thread,NULL);
    tcmg_log("%s", "[cccam] stopped");
}
