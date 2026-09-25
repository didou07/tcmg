#define MODULE_LOG_PREFIX "webif-service"
#include "service.h"
#include "../../src/core/config_state.h"
#include "../../src/config/runtime_access.h"
#include "../../src/core/runtime_state.h"
#include "../../src/core/utils.h"
#include "../../src/crypto/crypto.h"
#include "../../src/security/failban.h"
#include "../../src/stats/account_stats.h"
#include "../../src/pcsc/pcsc.h"
#include "../internal/proto.h"
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>
S_WEBIF_SERVER_STATS webif_server_stats(void)
{
    S_WEBIF_SERVER_STATS s;
    memset(&s, 0, sizeof(s));
    time_t now = time(NULL);

    pthread_rwlock_rdlock(&g_cfg.acc_lock);
    s.naccounts = g_cfg.naccounts;
    pthread_rwlock_unlock(&g_cfg.acc_lock);

    account_stats_global_snapshot(&s.cw_found, &s.cw_not);

    pthread_mutex_lock(&g_cfg.ban_lock);
    for (int i = 0; i < BAN_BUCKETS; i++)
        for (const S_BAN_ENTRY *b = g_cfg.ban_table[i]; b; b = b->next)
            if (b->until > now) s.nbans++;
    pthread_mutex_unlock(&g_cfg.ban_lock);

    s.ecm_total = s.cw_found + s.cw_not;
    s.hit_rate = s.ecm_total > 0
        ? (double)s.cw_found * 100.0 / (double)s.ecm_total
        : 0.0;
    s.active_conns = atomic_load(&g_active_conns);
    s.uptime_s = now - g_start_time;
    format_uptime(s.uptime_s, s.uptime_str, sizeof(s.uptime_str));
    return s;
}

int webif_pcsc_enabled(void)
{
    S_CONFIG_PCSC_READER_VIEW reader;
    return cfg_runtime_pcsc_reader_snapshot(&reader, 1) > 0 ? 1 : 0;
}

int webif_ban_snapshot(S_WEBIF_BAN_VIEW *out, size_t cap, S_WEBIF_BAN_STATS *stats)
{
    int n=0; time_t now=time(NULL); if(stats) memset(stats,0,sizeof(*stats));
    pthread_rwlock_rdlock(&g_cfg.acc_lock); if(stats){stats->max_fails=g_cfg.failban_max_fails>0?g_cfg.failban_max_fails:BAN_MAX_FAILS;stats->ban_secs=g_cfg.failban_ban_secs>0?g_cfg.failban_ban_secs:BAN_SECS;} pthread_rwlock_unlock(&g_cfg.acc_lock);
    pthread_mutex_lock(&g_cfg.ban_lock); for(int i=0;i<BAN_BUCKETS;i++) for(S_BAN_ENTRY *b=g_cfg.ban_table[i];b;b=b->next) if(b->until>now){if(stats){stats->active_bans++;stats->total_fails+=b->fails;} if(out && (size_t)n<cap){tcmg_strlcpy(out[n].ip,b->ip,sizeof(out[n].ip));out[n].fails=b->fails;out[n].until=b->until;n++;}} pthread_mutex_unlock(&g_cfg.ban_lock); return n;
}

int webif_ban_clear(const char *ip)
{
    int n=0; time_t now=time(NULL); pthread_mutex_lock(&g_cfg.ban_lock); uint32_t h=ip?ban_hash_pub(ip):0; if(ip&&*ip){for(S_BAN_ENTRY *b=g_cfg.ban_table[h];b;b=b->next)if(!strcmp(b->ip,ip)){if(b->until>now)n++;b->until=0;b->fails=0;}} pthread_mutex_unlock(&g_cfg.ban_lock); return n;
}

int webif_ban_clear_all(void)
{
    int n=0; time_t now=time(NULL); pthread_mutex_lock(&g_cfg.ban_lock); for(int i=0;i<BAN_BUCKETS;i++)for(S_BAN_ENTRY*b=g_cfg.ban_table[i];b;b=b->next){if(b->until>now)n++;b->until=0;b->fails=0;} pthread_mutex_unlock(&g_cfg.ban_lock); return n;
}

bool webif_auth_enabled(void)
{
    bool v; pthread_rwlock_rdlock(&g_cfg.acc_lock); v=g_cfg.webif_user[0]||g_cfg.webif_pass[0]; pthread_rwlock_unlock(&g_cfg.acc_lock); return v;
}

bool webif_auth_basic_valid(const char *header)
{
    if (!header) return false;
    S_WEBIF_CONFIG_VIEW c; webif_config_snapshot(&c);
    const char *p=strstr(header,"Basic "); if(!p) return false; p+=6;
    char got[512]; int n=0; while(*p&&*p!='\r'&&*p!='\n'&&*p!=' '&&n<(int)sizeof(got)-1)got[n++]=*p++; got[n]=0;
    char creds[256], expected[512]; snprintf(creds,sizeof(creds),"%s:%s",c.webif_user,c.webif_pass); b64_encode(creds,(int)strlen(creds),expected,sizeof(expected));
    return ct_streq(got,expected)!=0;
}

bool webif_credentials_valid(const char *user,const char *pass)
{
    S_WEBIF_CONFIG_VIEW c; webif_config_snapshot(&c); if(!c.webif_user[0]&&!c.webif_pass[0])return true; if(!user||!pass)return false; return ct_streq(user,c.webif_user)&&ct_streq(pass,c.webif_pass);
}

int webif_port(void){S_WEBIF_CONFIG_VIEW c;webif_config_snapshot(&c);return c.webif_port;}
void webif_bindaddr(char*out,size_t sz){S_WEBIF_CONFIG_VIEW c;webif_config_snapshot(&c);tcmg_strlcpy(out,c.webif_bindaddr,sz);}
int webif_max_refresh(void){S_WEBIF_CONFIG_VIEW c;webif_config_snapshot(&c);return c.webif_refresh;}
