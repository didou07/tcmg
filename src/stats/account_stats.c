#define MODULE_LOG_PREFIX "stats"
#include "account_stats.h"
#include "core/utils.h"
#include "log/log.h"
#include <string.h>
#include <time.h>
#include <stdatomic.h>

static _Atomic int64_t s_global_cw_found = 0;
static _Atomic int64_t s_global_cw_not = 0;

bool account_stats_init(S_ACCOUNT_STATS *stats)
{
    if (!stats) return false;
    memset(stats, 0, sizeof(*stats));
    if (pthread_mutex_init(&stats->lock, NULL) != 0) return false;
    atomic_init(&stats->global_tracked, 1);
    atomic_init(&stats->last_seen, 0);
    return true;
}

void account_stats_destroy(S_ACCOUNT_STATS *stats)
{
    if (!stats) return;
    pthread_mutex_destroy(&stats->lock);
}

void account_stats_mark_login(S_ACCOUNT *account, const char *ip)
{
    time_t now;
    if (!account) return;
    now = time(NULL);
    pthread_mutex_lock(&account->stats.lock);
    if (ip) tcmg_strlcpy(account->stats.last_ip, ip, sizeof(account->stats.last_ip));
    atomic_store(&account->stats.last_seen, now);
    if (account->stats.first_login == 0) account->stats.first_login = now;
    pthread_mutex_unlock(&account->stats.lock);
    log_set_user(account->user);
}

void account_stats_record_ecm(S_ACCOUNT *account, bool found, int64_t elapsed_ms)
{
    if (!account) return;
    if (elapsed_ms < 0) elapsed_ms = 0;
    pthread_mutex_lock(&account->stats.lock);
    account->stats.ecm_total++;
    atomic_store(&account->stats.last_seen, time(NULL));
    const bool tracked = atomic_load_explicit(&account->stats.global_tracked, memory_order_relaxed) != 0;
    if (found) {
        account->stats.cw_found++;
        if (tracked) atomic_fetch_add_explicit(&s_global_cw_found, 1, memory_order_relaxed);
        account->stats.cw_time_total_ms += elapsed_ms;
        if (account->stats.cw_time_min_ms == 0 || elapsed_ms < account->stats.cw_time_min_ms)
            account->stats.cw_time_min_ms = elapsed_ms;
        if (elapsed_ms > account->stats.cw_time_max_ms)
            account->stats.cw_time_max_ms = elapsed_ms;
    } else {
        account->stats.cw_not++;
        if (tracked) atomic_fetch_add_explicit(&s_global_cw_not, 1, memory_order_relaxed);
    }
    pthread_mutex_unlock(&account->stats.lock);
}

void account_stats_snapshot(const S_ACCOUNT *account, S_ACCOUNT_STATS_SNAPSHOT *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!account) return;
    pthread_mutex_lock((pthread_mutex_t *)&account->stats.lock);
    out->ecm_total = account->stats.ecm_total;
    out->cw_found = account->stats.cw_found;
    out->cw_not = account->stats.cw_not;
    out->cw_time_total_ms = account->stats.cw_time_total_ms;
    out->last_seen = atomic_load(&account->stats.last_seen);
    tcmg_strlcpy(out->last_ip, account->stats.last_ip, sizeof(out->last_ip));
    out->first_login = account->stats.first_login;
    out->cw_time_min_ms = account->stats.cw_time_min_ms;
    out->cw_time_max_ms = account->stats.cw_time_max_ms;
    pthread_mutex_unlock((pthread_mutex_t *)&account->stats.lock);
}

void account_stats_reset(S_ACCOUNT *account)
{
    if (!account) return;
    pthread_mutex_lock(&account->stats.lock);
    if (atomic_load_explicit(&account->stats.global_tracked, memory_order_relaxed)) {
        atomic_fetch_sub_explicit(&s_global_cw_found, account->stats.cw_found, memory_order_relaxed);
        atomic_fetch_sub_explicit(&s_global_cw_not, account->stats.cw_not, memory_order_relaxed);
    }
    account->stats.ecm_total = 0;
    account->stats.cw_found = 0;
    account->stats.cw_not = 0;
    account->stats.cw_time_total_ms = 0;
    account->stats.cw_time_min_ms = 0;
    account->stats.cw_time_max_ms = 0;
    account->stats.first_login = 0;
    atomic_store(&account->stats.last_seen, 0);
    account->stats.last_ip[0] = '\0';
    pthread_mutex_unlock(&account->stats.lock);
}

void account_stats_copy_runtime(S_ACCOUNT *dst, const S_ACCOUNT *src)
{
    S_ACCOUNT_STATS_SNAPSHOT snap;
    if (!dst || !src || dst == src) return;
    account_stats_snapshot(src, &snap);
    pthread_mutex_lock(&dst->stats.lock);
    if (atomic_load_explicit(&dst->stats.global_tracked, memory_order_relaxed)) {
        atomic_fetch_sub_explicit(&s_global_cw_found, dst->stats.cw_found, memory_order_relaxed);
        atomic_fetch_sub_explicit(&s_global_cw_not, dst->stats.cw_not, memory_order_relaxed);
        atomic_fetch_add_explicit(&s_global_cw_found, snap.cw_found, memory_order_relaxed);
        atomic_fetch_add_explicit(&s_global_cw_not, snap.cw_not, memory_order_relaxed);
    }
    dst->stats.ecm_total = snap.ecm_total;
    dst->stats.cw_found = snap.cw_found;
    dst->stats.cw_not = snap.cw_not;
    dst->stats.cw_time_total_ms = snap.cw_time_total_ms;
    atomic_store(&dst->stats.last_seen, snap.last_seen);
    tcmg_strlcpy(dst->stats.last_ip, snap.last_ip, sizeof(dst->stats.last_ip));
    dst->stats.first_login = snap.first_login;
    dst->stats.cw_time_min_ms = snap.cw_time_min_ms;
    dst->stats.cw_time_max_ms = snap.cw_time_max_ms;
    pthread_mutex_unlock(&dst->stats.lock);
}

void account_stats_global_snapshot(int64_t *cw_found, int64_t *cw_not)
{
    if (cw_found) *cw_found = atomic_load_explicit(&s_global_cw_found, memory_order_relaxed);
    if (cw_not)  *cw_not  = atomic_load_explicit(&s_global_cw_not, memory_order_relaxed);
}


void account_stats_global_remove(S_ACCOUNT *account)
{
    if (!account) return;
    pthread_mutex_lock(&account->stats.lock);
    if (atomic_exchange_explicit(&account->stats.global_tracked, 0, memory_order_acq_rel)) {
        atomic_fetch_sub_explicit(&s_global_cw_found, account->stats.cw_found, memory_order_relaxed);
        atomic_fetch_sub_explicit(&s_global_cw_not, account->stats.cw_not, memory_order_relaxed);
    }
    pthread_mutex_unlock(&account->stats.lock);
}
