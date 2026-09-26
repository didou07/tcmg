#define MODULE_LOG_PREFIX "ban"
#include "failban.h"
#include "../config/runtime_access.h"
#include "../core/config_state.h"
#include "../core/utils.h"
#include "../log/log.h"

uint32_t ban_hash_pub(const char *ip)
{
    uint32_t h = 2166136261u;
    for (; *ip; ip++)
        h = (h ^ (uint8_t)*ip) * 16777619u;
    return h & (BAN_BUCKETS - 1);
}


static bool ban_ip_allowed(const char *ip)
{
    S_CONFIG_FAILBAN_VIEW cfg;
    if (!ip || !ip[0]) return false;
    cfg_runtime_failban_snapshot(&cfg);
    if (!cfg.enabled) return true;
    if (!cfg.allowlist[0]) return false;

    char list[CFGVAL_LEN];
    tcmg_strlcpy(list, cfg.allowlist, sizeof(list));
    char *save = NULL;
    for (char *tok = strtok_r(list, ",; \t\r\n", &save); tok; tok = strtok_r(NULL, ",; \t\r\n", &save))
    {
        char *slash = strchr(tok, '/');
        if (!slash) {
            if (strncmp(tok, ip, MAXIPLEN) == 0) return true;
            continue;
        }
#if defined(AF_INET)
        *slash = '\0';
        char *end = NULL;
        errno = 0;
        long prefix = strtol(slash + 1, &end, 10);
        if (errno == ERANGE || end == slash + 1 || *end != '\0' || prefix < 0 || prefix > 32) continue;
        struct in_addr net4, ip4;
        if (inet_pton(AF_INET, tok, &net4) != 1 || inet_pton(AF_INET, ip, &ip4) != 1) continue;
        uint32_t mask = prefix == 0 ? 0u : htonl(0xFFFFFFFFu << (32 - prefix));
        if ((net4.s_addr & mask) == (ip4.s_addr & mask)) return true;
#endif
    }
    return false;
}

static S_BAN_ENTRY *ban_find_locked(const char *ip)
{
    S_BAN_ENTRY *e = g_cfg.ban_table[ban_hash_pub(ip)];
    for (; e; e = e->next)
        if (strncmp(e->ip, ip, MAXIPLEN) == 0)
            return e;
    return NULL;
}

static void ban_prune_locked(void)
{
    time_t now = time(NULL);
    for (int i = 0; i < BAN_BUCKETS; i++)
    {
        S_BAN_ENTRY **pp = &g_cfg.ban_table[i];
        while (*pp)
        {
            S_BAN_ENTRY *e = *pp;
            if (e->until > 0 && now >= e->until)
            {
                tcmg_log("pruned expired entry: ip=%s", e->ip);
                *pp = e->next;
                free(e);
            }
            else
            {
                pp = &e->next;
            }
        }
    }
}

bool ban_is_banned(const char *ip)
{
    S_CONFIG_FAILBAN_VIEW cfg;
    cfg_runtime_failban_snapshot(&cfg);
    if (!cfg.enabled || ban_ip_allowed(ip)) return false;
    bool   banned = false;
    time_t now    = time(NULL);

    pthread_mutex_lock(&g_cfg.ban_lock);
    ban_prune_locked();
    S_BAN_ENTRY *e = ban_find_locked(ip);
    if (e && e->until > 0 && now < e->until)
    {
        banned = true;
        tcmg_log("check: ip=%s BANNED fails=%d expires_in=%lds",
                     ip, e->fails, (long)(e->until - now));
    }
    pthread_mutex_unlock(&g_cfg.ban_lock);

    return banned;
}

void ban_record_fail(const char *ip)
{
    S_CONFIG_FAILBAN_VIEW cfg;
    cfg_runtime_failban_snapshot(&cfg);
    if (!cfg.enabled || ban_ip_allowed(ip)) return;
    pthread_mutex_lock(&g_cfg.ban_lock);

    S_BAN_ENTRY *e = ban_find_locked(ip);
    if (!e)
    {
        e = (S_BAN_ENTRY *)calloc(1, sizeof(S_BAN_ENTRY));
        if (!e) { pthread_mutex_unlock(&g_cfg.ban_lock); return; }
        tcmg_strlcpy(e->ip, ip, MAXIPLEN);
        uint32_t bucket = ban_hash_pub(ip);
        e->next                 = g_cfg.ban_table[bucket];
        g_cfg.ban_table[bucket] = e;
    }

    e->fails++;
    int max_fails = cfg.max_fails > 0 ? cfg.max_fails : BAN_MAX_FAILS;
    int ban_secs  = cfg.ban_secs  > 0 ? cfg.ban_secs  : BAN_SECS;
    int remaining = max_fails - e->fails;
    if (remaining > 0)
        tcmg_log("fail: ip=%s fail_count=%d/%d remaining_attempts=%d",
                     ip, e->fails, max_fails, remaining);
    else
    {
        e->until = time(NULL) + ban_secs;
        tcmg_log("TRIGGERED: ip=%s banned_for=%ds fail_count=%d/%d",
                 ip, ban_secs, e->fails, max_fails);
    }

    pthread_mutex_unlock(&g_cfg.ban_lock);
}

void ban_record_ok(const char *ip)
{
    if (!ip || !ip[0]) return;
    pthread_mutex_lock(&g_cfg.ban_lock);

    uint32_t     bucket = ban_hash_pub(ip);
    S_BAN_ENTRY **pp    = &g_cfg.ban_table[bucket];
    while (*pp)
    {
        if (strncmp((*pp)->ip, ip, MAXIPLEN) == 0)
        {
            S_BAN_ENTRY *e = *pp;
            tcmg_log("cleared: ip=%s (successful login -- entry removed)", ip);
            *pp = e->next;
            free(e);
            break;
        }
        pp = &(*pp)->next;
    }

    pthread_mutex_unlock(&g_cfg.ban_lock);
}

void ban_free_all(void)
{
    pthread_mutex_lock(&g_cfg.ban_lock);
    for (int i = 0; i < BAN_BUCKETS; i++)
    {
        S_BAN_ENTRY *e = g_cfg.ban_table[i];
        while (e)
        {
            S_BAN_ENTRY *next = e->next;
            free(e);
            e = next;
        }
        g_cfg.ban_table[i] = NULL;
    }
    pthread_mutex_unlock(&g_cfg.ban_lock);
}
