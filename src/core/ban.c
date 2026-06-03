#define MODULE_LOG_PREFIX "ban"
#include "tcmg.h"

uint32_t ban_hash_pub(const char *ip)
{
    uint32_t h = 2166136261u;
    for (; *ip; ip++)
        h = (h ^ (uint8_t)*ip) * 16777619u;
    return h & (BAN_BUCKETS - 1);
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
                tcmg_log_dbg(D_BAN, "pruned expired ban: %s (was banned for %ds)",
                             e->ip, BAN_SECS);
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
    bool   banned = false;
    time_t now    = time(NULL);

    pthread_mutex_lock(&g_cfg.ban_lock);
    ban_prune_locked();
    S_BAN_ENTRY *e = ban_find_locked(ip);
    if (e && e->until > 0 && now < e->until)
    {
        banned = true;
        tcmg_log_dbg(D_BAN, "%s is banned: fails=%d expires_in=%lds",
                     ip, e->fails, (long)(e->until - now));
    }
    pthread_mutex_unlock(&g_cfg.ban_lock);

    return banned;
}

void ban_record_fail(const char *ip)
{
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
    int remaining = BAN_MAX_FAILS - e->fails;
    if (remaining > 0)
        tcmg_log_dbg(D_BAN, "%s fail_count=%d/%d (%d attempt(s) before ban)",
                     ip, e->fails, BAN_MAX_FAILS, remaining);
    else
    {
        e->until = time(NULL) + BAN_SECS;
        tcmg_log("banned %s for %ds (fail_count=%d/%d)",
                 ip, BAN_SECS, e->fails, BAN_MAX_FAILS);
    }

    pthread_mutex_unlock(&g_cfg.ban_lock);
}

void ban_record_ok(const char *ip)
{
    pthread_mutex_lock(&g_cfg.ban_lock);

    uint32_t     bucket = ban_hash_pub(ip);
    S_BAN_ENTRY **pp    = &g_cfg.ban_table[bucket];
    while (*pp)
    {
        if (strncmp((*pp)->ip, ip, MAXIPLEN) == 0)
        {
            S_BAN_ENTRY *e = *pp;
            tcmg_log_dbg(D_BAN, "%s cleared from ban list (login OK)", ip);
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
