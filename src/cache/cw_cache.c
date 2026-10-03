#define MODULE_LOG_PREFIX "cache"
#include "cache_state.h"
#include "../core/utils.h"
#include "../platform/platform.h"
#include "../log/log.h"
#include "../crypto/crypto.h"
#include "cw_cache.h"
#include <stdatomic.h>

#define CW_INFLIGHT_SIZE 1024
#define CW_INFLIGHT_SHARDS 64
#define CW_INFLIGHT_WAYS (CW_INFLIGHT_SIZE / CW_INFLIGHT_SHARDS)

struct s_cw_inflight {
    uint8_t ecm_md5[16];
    uint8_t running;
    uint32_t refs;
    uint32_t generation;
    pthread_cond_t cond;
};

static struct s_cw_inflight s_inflight[CW_INFLIGHT_SIZE];
static pthread_mutex_t s_inflight_mtx[CW_INFLIGHT_SHARDS];
static pthread_once_t s_inflight_once = PTHREAD_ONCE_INIT;

static inline uint32_t inflight_shard(const uint8_t *md5)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < 16; i++)
        h = (h ^ md5[i]) * 16777619u;
    return h & (CW_INFLIGHT_SHARDS - 1);
}

static void inflight_init(void)
{
    for (int i = 0; i < CW_INFLIGHT_SHARDS; i++)
        pthread_mutex_init(&s_inflight_mtx[i], NULL);
    for (int i = 0; i < CW_INFLIGHT_SIZE; i++)
        pthread_cond_init(&s_inflight[i].cond, NULL);
}

static int inflight_find_locked(uint32_t shard, const uint8_t *ecm_md5)
{
    const int base = (int)(shard * CW_INFLIGHT_WAYS);
    for (int i = 0; i < CW_INFLIGHT_WAYS; i++) {
        struct s_cw_inflight *in = &s_inflight[base + i];
        if (in->running && ct_memeq(in->ecm_md5, ecm_md5, 16))
            return base + i;
    }
    return -1;
}

E_CW_INFLIGHT_BEGIN cw_inflight_begin(const uint8_t *ecm_md5,
                                      uint8_t *cw_out,
                                      const S_ACCOUNT *acc,
                                      bool require_group,
                                      S_CW_INFLIGHT_WAIT *wait,
                                      int32_t *groups_out, int32_t *ngroups_out)
{
    if (!ecm_md5 || !wait) return CW_INFLIGHT_LEADER;
    wait->slot = -1;
    wait->generation = 0;
    if (ngroups_out) *ngroups_out = 0;
    pthread_once(&s_inflight_once, inflight_init);

    if (cw_out && cw_cache_lookup_groups(ecm_md5, cw_out, acc, require_group,
                                         groups_out, ngroups_out))
        return CW_INFLIGHT_HIT;

    const uint32_t shard = inflight_shard(ecm_md5);
    pthread_mutex_lock(&s_inflight_mtx[shard]);

    int existing = inflight_find_locked(shard, ecm_md5);
    if (existing >= 0) {
        struct s_cw_inflight *in = &s_inflight[existing];
        in->refs++;
        wait->slot = existing;
        wait->generation = in->generation;
        pthread_mutex_unlock(&s_inflight_mtx[shard]);
        return CW_INFLIGHT_WAIT;
    }

    int free_slot = -1;
    const int base = (int)(shard * CW_INFLIGHT_WAYS);
    for (int i = 0; i < CW_INFLIGHT_WAYS; i++) {
        int slot = base + i;
        if (!s_inflight[slot].running && s_inflight[slot].refs == 0) {
            free_slot = slot;
            break;
        }
    }

    if (free_slot >= 0) {
        struct s_cw_inflight *in = &s_inflight[free_slot];
        in->generation++;
        if (in->generation == 0) in->generation = 1;
        memcpy(in->ecm_md5, ecm_md5, 16);
        in->running = 1;
        in->refs = 1;
        wait->slot = free_slot;
        wait->generation = in->generation;
        pthread_mutex_unlock(&s_inflight_mtx[shard]);
        return CW_INFLIGHT_LEADER;
    }

    pthread_mutex_unlock(&s_inflight_mtx[shard]);
    return CW_INFLIGHT_LEADER;
}

bool cw_inflight_wait(S_CW_INFLIGHT_WAIT *wait,
                      const uint8_t *ecm_md5,
                      uint8_t *cw_out,
                      const S_ACCOUNT *acc,
                      bool require_group,
                      int32_t *groups_out, int32_t *ngroups_out)
{
    if (!wait || !ecm_md5 || !cw_out || wait->slot < 0 || wait->slot >= CW_INFLIGHT_SIZE)
        return false;
    if (ngroups_out) *ngroups_out = 0;

    pthread_once(&s_inflight_once, inflight_init);
    const uint32_t shard = inflight_shard(ecm_md5);
    pthread_mutex_lock(&s_inflight_mtx[shard]);
    struct s_cw_inflight *in = &s_inflight[wait->slot];

    if (in->generation != wait->generation || !in->running || in->refs == 0 ||
        !ct_memeq(in->ecm_md5, ecm_md5, 16)) {
        pthread_mutex_unlock(&s_inflight_mtx[shard]);
        return false;
    }

    while (in->running)
        pthread_cond_wait(&in->cond, &s_inflight_mtx[shard]);

    if (in->refs > 0) in->refs--;
    if (in->refs == 0) secure_zero(in->ecm_md5, sizeof(in->ecm_md5));
    pthread_mutex_unlock(&s_inflight_mtx[shard]);

    return cw_cache_lookup_groups(ecm_md5, cw_out, acc, require_group,
                                   groups_out, ngroups_out);
}

void cw_inflight_complete(const uint8_t *ecm_md5, bool success)
{
    (void)success;
    if (!ecm_md5) return;
    pthread_once(&s_inflight_once, inflight_init);

    const uint32_t shard = inflight_shard(ecm_md5);
    pthread_mutex_lock(&s_inflight_mtx[shard]);
    int idx = inflight_find_locked(shard, ecm_md5);
    if (idx < 0) {
        pthread_mutex_unlock(&s_inflight_mtx[shard]);
        return;
    }

    struct s_cw_inflight *in = &s_inflight[idx];
    in->running = 0;
    pthread_cond_broadcast(&in->cond);
    if (in->refs > 0) in->refs--;
    if (in->refs == 0) secure_zero(in->ecm_md5, sizeof(in->ecm_md5));
    pthread_mutex_unlock(&s_inflight_mtx[shard]);
}


static _Atomic uint64_t s_cache_seq;

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
            if (e->groups[j] == groups[i]) { exists = true; break; }
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
        if (cw_cache_entry_expired(e, now_ms)) { cw_cache_entry_clear(e); continue; }
        if (!ct_memeq(e->ecm_md5, ecm_md5, 16)) continue;
        if (require_group && !account_has_cached_group(acc, e)) continue;
        if (!best || e->seq > best->seq ||
            (e->seq == best->seq && e->last_used_ms > best->last_used_ms))
            best = e;
    }
    return best;
}

bool cw_cache_lookup_groups(const uint8_t *ecm_md5, uint8_t *cw_out,
                            const S_ACCOUNT *acc, bool require_group,
                            int32_t *groups_out, int32_t *ngroups_out)
{
    if (!ecm_md5 || !cw_out) return false;
    if (ngroups_out) *ngroups_out = 0;
    const uint32_t bucket = cw_bucket(ecm_md5);
    const uint32_t shard = cw_shard(bucket);
    const uint64_t now_ms = (uint64_t)tcmg_mono_ms();
    bool hit = false;
    pthread_mutex_lock(&g_cw_cache_mtx[shard]);
    S_CW_CACHE_ENTRY *e = cw_cache_find_best_locked(ecm_md5, acc, require_group, now_ms);
    if (e) {
        memcpy(cw_out, e->cw, CW_LEN);
        if (groups_out && ngroups_out) {
            int32_t n = e->ngroups;
            if (n < 0) n = 0;
            if (n > MAX_GROUPS_PER_READER) n = MAX_GROUPS_PER_READER;
            if (n > 0) memcpy(groups_out, e->groups, (size_t)n * sizeof(groups_out[0]));
            *ngroups_out = n;
        }
        e->last_used_ms = now_ms;
        hit = true;
    }
    pthread_mutex_unlock(&g_cw_cache_mtx[shard]);
    return hit;
}

bool cw_cache_lookup(const uint8_t *ecm_md5, uint8_t *cw_out,
                     const S_ACCOUNT *acc, bool require_group)
{
    return cw_cache_lookup_groups(ecm_md5, cw_out, acc, require_group, NULL, NULL);
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
                     "CW updated bucket=%u count=%u groups=%d",
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
                 "CW stored bucket=%u count=%u groups=%d",
                 bucket, stored_count, stored_groups);
}
