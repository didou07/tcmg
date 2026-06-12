#define MODULE_LOG_PREFIX "cache"
#include "../../globals.h"

static inline uint32_t cw_idx(const uint8_t *md5)
{
    uint32_t h = 2166136261u;
    h = (h ^ md5[0]) * 16777619u;
    h = (h ^ md5[1]) * 16777619u;
    h = (h ^ md5[2]) * 16777619u;
    h = (h ^ md5[3]) * 16777619u;
    return h & (CW_CACHE_SIZE - 1);
}

bool cw_cache_lookup(const uint8_t *ecm_md5, uint8_t *cw_out)
{
    uint32_t idx   = cw_idx(ecm_md5);
    uint32_t shard = idx & (CW_CACHE_SHARDS - 1);
    bool hit = false;
    pthread_mutex_lock(&g_cw_cache_mtx[shard]);
    S_CW_CACHE_ENTRY *e = &g_cw_cache[idx];
    if (e->valid && ct_memeq(e->ecm_md5, ecm_md5, 16) &&
        (time(NULL) - e->ts) < CW_CACHE_TTL_S)
    {
        memcpy(cw_out, e->cw, CW_LEN);
        hit = true;
    }
    pthread_mutex_unlock(&g_cw_cache_mtx[shard]);
    return hit;
}

void cw_cache_store(const uint8_t *ecm_md5, const uint8_t *cw)
{
    uint32_t idx   = cw_idx(ecm_md5);
    uint32_t shard = idx & (CW_CACHE_SHARDS - 1);
    pthread_mutex_lock(&g_cw_cache_mtx[shard]);
    S_CW_CACHE_ENTRY *e = &g_cw_cache[idx];
    memcpy(e->ecm_md5, ecm_md5, 16);
    memcpy(e->cw,      cw,      CW_LEN);
    e->ts    = time(NULL);
    e->valid = 1;
    pthread_mutex_unlock(&g_cw_cache_mtx[shard]);
    tcmg_log_dbg(D_CCCAM|D_NEWCAMD, "cw cache stored slot=%u shard=%u", idx, shard);
}
