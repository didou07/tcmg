#define MODULE_LOG_PREFIX "cache"
#include "cache_state.h"
#include "../core/utils.h"
#include "../log/log.h"
#include "../crypto/crypto.h"
#include "cw_cache.h"

static inline uint32_t cw_idx(const uint8_t *md5)
{
    uint32_t h = 2166136261u;
    h = (h ^ md5[0]) * 16777619u;
    h = (h ^ md5[1]) * 16777619u;
    h = (h ^ md5[2]) * 16777619u;
    h = (h ^ md5[3]) * 16777619u;
    return h & (CW_CACHE_SIZE - 1);
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

bool cw_cache_lookup(const uint8_t *ecm_md5, uint8_t *cw_out,
                     const S_ACCOUNT *acc, bool require_group)
{
    uint32_t idx   = cw_idx(ecm_md5);
    uint32_t shard = idx & (CW_CACHE_SHARDS - 1);
    bool hit = false;

    pthread_mutex_lock(&g_cw_cache_mtx[shard]);
    S_CW_CACHE_ENTRY *e = &g_cw_cache[idx];
    if (e->valid && ct_memeq(e->ecm_md5, ecm_md5, 16) &&
        (time(NULL) - e->ts) < CW_CACHE_TTL_S &&
        (!require_group ? true : account_has_cached_group(acc, e)))
    {
        memcpy(cw_out, e->cw, CW_LEN);
        hit = true;
    }
    pthread_mutex_unlock(&g_cw_cache_mtx[shard]);
    return hit;
}

void cw_cache_store_groups(const uint8_t *ecm_md5, const uint8_t *cw,
                           const int32_t *groups, int32_t ngroups)
{
    uint32_t idx   = cw_idx(ecm_md5);
    uint32_t shard = idx & (CW_CACHE_SHARDS - 1);
    int32_t stored_groups = 0;
    pthread_mutex_lock(&g_cw_cache_mtx[shard]);
    S_CW_CACHE_ENTRY *e = &g_cw_cache[idx];
    memcpy(e->ecm_md5, ecm_md5, 16);
    memcpy(e->cw, cw, CW_LEN);
    e->ts = time(NULL);
    e->valid = 1;
    e->scoped = 1;
    e->ngroups = (ngroups < MAX_GROUPS_PER_READER) ? ngroups : MAX_GROUPS_PER_READER;
    stored_groups = e->ngroups;
    memset(e->groups, 0, sizeof(e->groups));
    if (groups && e->ngroups > 0)
        memcpy(e->groups, groups, (size_t)e->ngroups * sizeof(e->groups[0]));
    pthread_mutex_unlock(&g_cw_cache_mtx[shard]);
    tcmg_log_dbg(D_CCCAM|D_NEWCAMD, "cw cache stored scoped slot=%u groups=%d", idx, stored_groups);
}
