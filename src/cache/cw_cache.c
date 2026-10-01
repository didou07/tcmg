#define MODULE_LOG_PREFIX "cache"
#include "cache_state.h"
#include "../core/utils.h"
#include "../platform/platform.h"
#include "../log/log.h"
#include "../crypto/crypto.h"
#include "cw_cache.h"
#include <stdatomic.h>

#define CW_PENDING_SIZE 1024
#define CW_PENDING_RUNNING 1

struct s_cw_pending {
    uint8_t  ecm_md5[16];
    int32_t  reader_index;
    uint8_t  running;
    uint8_t  success;
    uint32_t refs;
    uint32_t generation;
    pthread_cond_t cond;
};

static struct s_cw_pending s_pending[CW_PENDING_SIZE];
static pthread_mutex_t s_pending_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t s_pending_once = PTHREAD_ONCE_INIT;
static _Atomic uint64_t s_cache_seq;

static void pending_init(void)
{
    for (int i = 0; i < CW_PENDING_SIZE; i++)
        pthread_cond_init(&s_pending[i].cond, NULL);
}

static inline uint32_t cw_bucket(const uint8_t *md5)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < 16; i++)
        h = (h ^ md5[i]) * 16777619u;
    return h % CW_CACHE_BUCKETS;
}

static inline uint32_t cw_shard(uint32_t bucket)
{
    return bucket & (CW_CACHE_SHARDS - 1);
}

static inline bool cw_cache_entry_expired(const S_CW_CACHE_ENTRY *e, uint64_t now_ms)
{
    if (!e || !e->valid) return true;
    return now_ms - (uint64_t)e->ts_ms >= (uint64_t)CW_CACHE_TTL_S * 1000ULL;
}

static inline void cw_cache_entry_clear(S_CW_CACHE_ENTRY *e)
{
    if (!e) return;
    secure_zero(e, sizeof(*e));
}

static bool account_has_cached_group(const S_ACCOUNT *acc, const S_CW_CACHE_ENTRY *e)
{
    if (!acc || !e || !e->scoped || e->ngroups <= 0) return false;

    if (acc->ngroups > 0) {
        for (int i = 0; i < acc->ngroups; i++)
            for (int j = 0; j < e->ngroups; j++)
                if (acc->groups[i] == e->groups[j]) return true;
        return false;
    }

    for (int j = 0; j < e->ngroups; j++)
        if (acc->group == e->groups[j]) return true;
    return false;
}

static void merge_groups(S_CW_CACHE_ENTRY *e, const int32_t *groups, int32_t ngroups)
{
    if (!e || !groups || ngroups <= 0) return;
    if (ngroups > MAX_GROUPS_PER_READER) ngroups = MAX_GROUPS_PER_READER;

    for (int32_t i = 0; i < ngroups; i++) {
        bool exists = false;
        for (int32_t j = 0; j < e->ngroups; j++) {
            if (e->groups[j] == groups[i]) {
                exists = true;
                break;
            }
        }
        if (!exists && e->ngroups < MAX_GROUPS_PER_READER)
            e->groups[e->ngroups++] = groups[i];
    }
}

static S_CW_CACHE_ENTRY *cw_cache_find_best_locked(const uint8_t *ecm_md5,
                                                   const S_ACCOUNT *acc,
                                                   bool require_group,
                                                   uint64_t now_ms)
{
    const uint32_t bucket = cw_bucket(ecm_md5);
    const uint32_t base = bucket * CW_CACHE_WAYS;
    S_CW_CACHE_ENTRY *best = NULL;

    for (uint32_t way = 0; way < CW_CACHE_WAYS; way++) {
        S_CW_CACHE_ENTRY *e = &g_cw_cache[base + way];
        if (!e->valid) continue;
        if (cw_cache_entry_expired(e, now_ms)) {
            cw_cache_entry_clear(e);
            continue;
        }
        if (!ct_memeq(e->ecm_md5, ecm_md5, 16)) continue;
        if (require_group && !account_has_cached_group(acc, e)) continue;

        if (!best || e->seq > best->seq ||
            (e->seq == best->seq && e->last_used_ms > best->last_used_ms))
            best = e;
    }
    return best;
}

bool cw_cache_lookup(const uint8_t *ecm_md5, uint8_t *cw_out,
                     const S_ACCOUNT *acc, bool require_group)
{
    if (!ecm_md5 || !cw_out) return false;
    const uint32_t bucket = cw_bucket(ecm_md5);
    const uint32_t shard = cw_shard(bucket);
    const uint64_t now_ms = (uint64_t)tcmg_mono_ms();
    bool hit = false;

    pthread_mutex_lock(&g_cw_cache_mtx[shard]);
    S_CW_CACHE_ENTRY *e = cw_cache_find_best_locked(ecm_md5, acc, require_group, now_ms);
    if (e) {
        memcpy(cw_out, e->cw, CW_LEN);
        e->last_used_ms = now_ms;
        hit = true;
    }
    pthread_mutex_unlock(&g_cw_cache_mtx[shard]);
    return hit;
}

static int pending_find_locked(const uint8_t *ecm_md5, int32_t reader_index)
{
    for (int i = 0; i < CW_PENDING_SIZE; i++) {
        if (s_pending[i].running && s_pending[i].reader_index == reader_index &&
            ct_memeq(s_pending[i].ecm_md5, ecm_md5, 16))
            return i;
    }
    return -1;
}

E_CW_CACHE_BEGIN cw_cache_begin_reader(const uint8_t *ecm_md5, int32_t reader_index,
                                       uint8_t *cw_out, const S_ACCOUNT *acc,
                                       bool require_group, S_CW_CACHE_WAIT *wait)
{
    if (!ecm_md5 || !wait) return CW_CACHE_BEGIN_LEADER;
    wait->slot = -1;
    wait->generation = 0;
    wait->reader_index = reader_index;
    pthread_once(&s_pending_once, pending_init);

    if (cw_cache_lookup(ecm_md5, cw_out, acc, require_group))
        return CW_CACHE_BEGIN_HIT;

    pthread_mutex_lock(&s_pending_mtx);

    if (cw_cache_lookup(ecm_md5, cw_out, acc, require_group)) {
        pthread_mutex_unlock(&s_pending_mtx);
        return CW_CACHE_BEGIN_HIT;
    }

    int existing = pending_find_locked(ecm_md5, reader_index);
    if (existing >= 0) {
        struct s_cw_pending *p = &s_pending[existing];
        p->refs++;
        wait->slot = existing;
        wait->generation = p->generation;
        uint32_t refs = p->refs;
        pthread_mutex_unlock(&s_pending_mtx);
        tcmg_log_dbg(D_CCCAM|D_NEWCAMD,
                     "cw cache pending join reader=%d slot=%d refs=%u",
                     reader_index, existing, refs);
        return CW_CACHE_BEGIN_WAIT;
    }

    int free_slot = -1;
    for (int i = 0; i < CW_PENDING_SIZE; i++) {
        if (!s_pending[i].running && s_pending[i].refs == 0) {
            free_slot = i;
            break;
        }
    }

    if (free_slot >= 0) {
        struct s_cw_pending *p = &s_pending[free_slot];
        p->generation++;
        if (p->generation == 0) p->generation = 1;
        memcpy(p->ecm_md5, ecm_md5, 16);
        p->reader_index = reader_index;
        p->running = CW_PENDING_RUNNING;
        p->success = 0;
        p->refs = 1;
        wait->slot = free_slot;
        wait->generation = p->generation;
        pthread_mutex_unlock(&s_pending_mtx);
        tcmg_log_dbg(D_CCCAM|D_NEWCAMD,
                     "cw cache pending leader reader=%d slot=%d",
                     reader_index, free_slot);
        return CW_CACHE_BEGIN_LEADER;
    }

    pthread_mutex_unlock(&s_pending_mtx);
    tcmg_log_dbg(D_CCCAM|D_NEWCAMD,
                 "cw cache pending table full reader=%d -> reader fallback",
                 reader_index);
    return CW_CACHE_BEGIN_LEADER;
}

bool cw_cache_wait(S_CW_CACHE_WAIT *wait, const uint8_t *ecm_md5, uint8_t *cw_out,
                   const S_ACCOUNT *acc, bool require_group)
{
    if (!wait || !ecm_md5 || !cw_out || wait->slot < 0 || wait->slot >= CW_PENDING_SIZE)
        return false;

    pthread_once(&s_pending_once, pending_init);
    pthread_mutex_lock(&s_pending_mtx);
    struct s_cw_pending *p = &s_pending[wait->slot];

    if (p->generation != wait->generation || p->reader_index != wait->reader_index || p->refs == 0) {
        pthread_mutex_unlock(&s_pending_mtx);
        return false;
    }

    while (p->running)
        pthread_cond_wait(&p->cond, &s_pending_mtx);

    if (p->refs > 0) p->refs--;
    if (p->refs == 0) {
        secure_zero(p->ecm_md5, sizeof(p->ecm_md5));
        p->success = 0;
    }
    pthread_mutex_unlock(&s_pending_mtx);

    if (cw_cache_lookup(ecm_md5, cw_out, acc, require_group))
        return true;
    return false;
}

void cw_cache_complete_reader(const uint8_t *ecm_md5, int32_t reader_index, bool success)
{
    if (!ecm_md5) return;
    pthread_once(&s_pending_once, pending_init);

    pthread_mutex_lock(&s_pending_mtx);
    int idx = pending_find_locked(ecm_md5, reader_index);
    if (idx < 0) {
        pthread_mutex_unlock(&s_pending_mtx);
        return;
    }

    struct s_cw_pending *p = &s_pending[idx];
    p->success = success ? 1 : 0;
    p->running = 0;
    pthread_cond_broadcast(&p->cond);
    if (p->refs > 0) p->refs--;
    uint32_t waiters = p->refs;
    if (p->refs == 0) {
        secure_zero(p->ecm_md5, sizeof(p->ecm_md5));
        p->success = 0;
    }
    pthread_mutex_unlock(&s_pending_mtx);

    tcmg_log_dbg(D_CCCAM|D_NEWCAMD,
                 "cw cache pending complete reader=%d success=%d slot=%d waiters=%u",
                 reader_index, success ? 1 : 0, idx, waiters);
}

void cw_cache_store_groups(const uint8_t *ecm_md5, const uint8_t *cw,
                           const int32_t *groups, int32_t ngroups)
{
    if (!ecm_md5 || !cw) return;
    const uint32_t bucket = cw_bucket(ecm_md5);
    const uint32_t shard = cw_shard(bucket);
    const uint64_t now_ms = (uint64_t)tcmg_mono_ms();
    const uint32_t base = bucket * CW_CACHE_WAYS;
    S_CW_CACHE_ENTRY *e = NULL;

    pthread_mutex_lock(&g_cw_cache_mtx[shard]);

    for (uint32_t way = 0; way < CW_CACHE_WAYS; way++) {
        S_CW_CACHE_ENTRY *candidate = &g_cw_cache[base + way];
        if (!candidate->valid) continue;
        if (cw_cache_entry_expired(candidate, now_ms)) {
            cw_cache_entry_clear(candidate);
            continue;
        }
        if (!ct_memeq(candidate->ecm_md5, ecm_md5, 16) ||
            !ct_memeq(candidate->cw, cw, CW_LEN))
            continue;

        e = candidate;
        break;
    }

    if (e) {
        merge_groups(e, groups, ngroups);
        if (e->count < UINT32_MAX) e->count++;
        e->ts_ms = (int64_t)now_ms;
        e->last_used_ms = now_ms;
        e->seq = atomic_fetch_add_explicit(&s_cache_seq, 1, memory_order_relaxed) + 1;
        uint32_t updated_count = e->count;
        int32_t updated_groups = e->ngroups;
        pthread_mutex_unlock(&g_cw_cache_mtx[shard]);
        tcmg_log_dbg(D_CCCAM|D_NEWCAMD,
                     "cw cache updated bucket=%u count=%u groups=%d",
                     bucket, updated_count, updated_groups);
        return;
    }

    uint32_t replace_way = 0;
    uint64_t oldest = UINT64_MAX;
    for (uint32_t way = 0; way < CW_CACHE_WAYS; way++) {
        S_CW_CACHE_ENTRY *candidate = &g_cw_cache[base + way];
        if (!candidate->valid) {
            replace_way = way;
            break;
        }
        if (cw_cache_entry_expired(candidate, now_ms)) {
            replace_way = way;
            cw_cache_entry_clear(candidate);
            break;
        }
        if (candidate->last_used_ms < oldest) {
            oldest = candidate->last_used_ms;
            replace_way = way;
        }
    }
    e = &g_cw_cache[base + replace_way];
    cw_cache_entry_clear(e);
    memcpy(e->ecm_md5, ecm_md5, 16);
    memcpy(e->cw, cw, CW_LEN);
    e->ts_ms = (int64_t)now_ms;
    e->last_used_ms = now_ms;
    e->seq = atomic_fetch_add_explicit(&s_cache_seq, 1, memory_order_relaxed) + 1;
    e->count = 1;
    e->valid = 1;
    e->scoped = 1;
    e->ngroups = 0;
    merge_groups(e, groups, ngroups);

    int32_t stored_groups = e->ngroups;
    uint32_t stored_count = e->count;
    pthread_mutex_unlock(&g_cw_cache_mtx[shard]);
    tcmg_log_dbg(D_CCCAM|D_NEWCAMD,
                 "cw cache stored bucket=%u count=%u groups=%d",
                 bucket, stored_count, stored_groups);
}
