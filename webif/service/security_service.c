#define MODULE_LOG_PREFIX "webif"
#include "service.h"
#include "../../src/core/config_state.h"
#include "../../src/config/runtime_access.h"
#include "../../src/core/runtime_state.h"
#include "../../src/core/utils.h"
#include "../../src/crypto/crypto.h"
#include "../../src/security/failban.h"
#include "../../src/stats/account_stats.h"
#include "../../src/pcsc/pcsc.h"
#include "../../src/reader/protocol.h"
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
    S_READER readers[MAX_READERS];
    int n = cfg_runtime_reader_snapshot(readers, MAX_READERS);
    for (int i = 0; i < n; i++) {
        if (!readers[i].enabled) continue;
        if (reader_protocol_kind(readers[i].protocol) == READER_PROTOCOL_CARD &&
            strcasecmp(readers[i].protocol, "pcsc") == 0)
            return 1;
    }
    return 0;
}

int webif_ban_snapshot(S_WEBIF_BAN_VIEW *out, size_t cap, S_WEBIF_BAN_STATS *stats)
{
    int n = 0;
    time_t now = time(NULL);
    if (stats) memset(stats, 0, sizeof(*stats));

    pthread_rwlock_rdlock(&g_cfg.acc_lock);
    if (stats) {
        stats->max_fails = g_cfg.failban_max_fails > 0 ? g_cfg.failban_max_fails : BAN_MAX_FAILS;
        stats->ban_secs = g_cfg.failban_ban_secs > 0 ? g_cfg.failban_ban_secs : BAN_SECS;
    }
    pthread_rwlock_unlock(&g_cfg.acc_lock);

    pthread_mutex_lock(&g_cfg.ban_lock);
    for (int i = 0; i < BAN_BUCKETS; i++) {
        for (S_BAN_ENTRY *b = g_cfg.ban_table[i]; b; b = b->next) {
            if (b->until <= now) continue;
            if (stats) {
                stats->active_bans++;
                stats->total_fails += b->fails;
            }
            if (out && (size_t)n < cap) {
                tcmg_strlcpy(out[n].ip, b->ip, sizeof(out[n].ip));
                out[n].fails = b->fails;
                out[n].until = b->until;
                n++;
            }
        }
    }
    pthread_mutex_unlock(&g_cfg.ban_lock);
    return n;
}

int webif_ban_clear(const char *ip)
{
    int n = 0;
    time_t now = time(NULL);
    if (!ip || !*ip) return 0;

    uint32_t h = ban_hash_pub(ip);
    pthread_mutex_lock(&g_cfg.ban_lock);
    for (S_BAN_ENTRY *b = g_cfg.ban_table[h]; b; b = b->next) {
        if (strcmp(b->ip, ip) != 0) continue;
        if (b->until > now) n++;
        b->until = 0;
        b->fails = 0;
    }
    pthread_mutex_unlock(&g_cfg.ban_lock);
    return n;
}

int webif_ban_clear_all(void)
{
    int n = 0;
    time_t now = time(NULL);
    pthread_mutex_lock(&g_cfg.ban_lock);
    for (int i = 0; i < BAN_BUCKETS; i++) {
        for (S_BAN_ENTRY *b = g_cfg.ban_table[i]; b; b = b->next) {
            if (b->until > now) n++;
            b->until = 0;
            b->fails = 0;
        }
    }
    pthread_mutex_unlock(&g_cfg.ban_lock);
    return n;
}

bool webif_auth_enabled(void)
{
    bool enabled;
    pthread_rwlock_rdlock(&g_cfg.acc_lock);
    enabled = g_cfg.webif_user[0] || g_cfg.webif_pass[0];
    pthread_rwlock_unlock(&g_cfg.acc_lock);
    return enabled;
}

bool webif_auth_basic_valid(const char *header)
{
    if (!header) return false;

    S_WEBIF_CONFIG_VIEW c;
    if (!webif_config_snapshot(&c)) return false;

    const char *p = strstr(header, "Basic ");
    if (!p) return false;
    p += 6;

    char got[512];
    size_t n = 0;
    while (*p && *p != '\r' && *p != '\n' && *p != ' ' && n + 1 < sizeof(got))
        got[n++] = *p++;
    got[n] = '\0';

    char creds[256];
    char expected[512];
    snprintf(creds, sizeof(creds), "%s:%s", c.webif_user, c.webif_pass);
    b64_encode(creds, (int)strlen(creds), expected, sizeof(expected));
    return ct_streq(got, expected) != 0;
}

bool webif_credentials_valid(const char *user, const char *pass)
{
    S_WEBIF_CONFIG_VIEW c;
    if (!webif_config_snapshot(&c)) return false;
    if (!c.webif_user[0] && !c.webif_pass[0]) return true;
    if (!user || !pass) return false;
    return ct_streq(user, c.webif_user) && ct_streq(pass, c.webif_pass);
}

int webif_port(void)
{
    S_WEBIF_CONFIG_VIEW c;
    return webif_config_snapshot(&c) ? c.webif_port : 0;
}

void webif_bindaddr(char *out, size_t sz)
{
    if (!out || sz == 0) return;
    S_WEBIF_CONFIG_VIEW c;
    if (!webif_config_snapshot(&c)) {
        out[0] = '\0';
        return;
    }
    tcmg_strlcpy(out, c.webif_bindaddr, sz);
}

int webif_max_refresh(void)
{
    S_WEBIF_CONFIG_VIEW c;
    return webif_config_snapshot(&c) ? c.webif_refresh : 0;
}
