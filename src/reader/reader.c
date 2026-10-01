#define MODULE_LOG_PREFIX "reader"
#include "reader.h"
#include "core/config_state.h"
#include "ecm/request.h"
#include "core/utils.h"
#include "log/log.h"
#include "protocol.h"
#include "rules.h"
#include "result.h"
#include "stats.h"
#include "../cache/cw_cache.h"
#include "../crypto/crypto.h"
#include <pthread.h>

static pthread_once_t s_reader_cache_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t s_reader_cache_mtx[MAX_READERS];

static void reader_cache_locks_init(void)
{
    for (int i = 0; i < MAX_READERS; i++)
        pthread_mutex_init(&s_reader_cache_mtx[i], NULL);
}

static bool reader_is_internal(const S_READER *reader)
{
    return reader && strcasecmp(reader->protocol, "internal") == 0;
}

static int failure_rank(E_READER_FAILURE failure)
{
    switch (failure) {
    case READER_FAILURE_TRANSPORT_ERROR: return 3;
    case READER_FAILURE_CARD_ERROR:
    case READER_FAILURE_READER_ERROR: return 2;
    case READER_FAILURE_NOT_FOUND: return 1;
    default: return 0;
    }
}

int32_t reader_dispatch_ecm(const S_ECM_REQUEST *request, S_READER_RESULT *result)
{
    S_READER readers[MAX_READERS];
    int indices[MAX_READERS];
    int n = 0;

    if (result) reader_result_init(result);
    if (!request || !request->account || !request->ecm || !request->cw ||
        request->ecm_len <= 0 || request->ecm_len > 255)
        return -1;

    pthread_rwlock_rdlock(&g_cfg.acc_lock);
    for (int i = 0; i < MAX_READERS; i++) {
        if (!g_cfg.readers[i].in_use) continue;
        readers[n] = g_cfg.readers[i];
        indices[n] = i;
        n++;
    }
    pthread_rwlock_unlock(&g_cfg.acc_lock);

    uint8_t ecm_md5[TCMG_ECM_MD5_LEN];
    bool attempted = false;
    bool whitelist_rejected = false;
    E_READER_FAILURE failure_summary = READER_FAILURE_NONE;

    crypt_md5_hash(request->ecm, (size_t)request->ecm_len, ecm_md5);
    pthread_once(&s_reader_cache_once, reader_cache_locks_init);

    for (int i = 0; i < n; i++) {
        E_READER_RULE_RESULT rule = reader_rule_check(&readers[i], request->account,
                                                       request->caid, request->sid,
                                                       request->ecm_len);
        if (rule != READER_RULE_ALLOW) {
            if (rule == READER_RULE_ECM_WHITELIST) {
                whitelist_rejected = true;
                tcmg_log_dbg(D_READER,
                             "ECM rejected reader='%s' caid=%04X sid=%04X len=%d whitelist=%d",
                             readers[i].label, request->caid, request->sid,
                             request->ecm_len, readers[i].ecm_whitelist);
            }
            continue;
        }
        attempted = true;

        const S_READER_PROTOCOL *protocol = reader_protocol_find(readers[i].protocol);
        if (!protocol || !protocol->do_ecm) {
            tcmg_log_dbg(D_READER, "index=%d label='%s' unknown protocol=%s",
                         indices[i], readers[i].label, readers[i].protocol);
            continue;
        }

        pthread_mutex_t *gate = reader_is_internal(&readers[i]) ? NULL : &s_reader_cache_mtx[indices[i]];
        S_CW_CACHE_WAIT cache_wait = { .slot = -1, .generation = 0, .reader_index = indices[i] };
        E_CW_CACHE_BEGIN cache_begin = CW_CACHE_BEGIN_LEADER;

        if (cw_cache_lookup(ecm_md5, request->cw, request->account, true)) {
            if (result) {
                result->status = EMU_OK;
                result->reader_index = indices[i];
                result->cache_hit = 1;
                int ngrp = readers[i].ngroups;
                if (ngrp <= 0) {
                    result->groups[0] = 1;
                    result->ngroups = 1;
                } else {
                    if (ngrp > MAX_GROUPS_PER_READER) ngrp = MAX_GROUPS_PER_READER;
                    memcpy(result->groups, readers[i].groups,
                           (size_t)ngrp * sizeof(result->groups[0]));
                    result->ngroups = ngrp;
                }
            }
            secure_zero(ecm_md5, sizeof(ecm_md5));
            tcmg_log_dbg(D_READER, "cache hit before reader index=%d label='%s' caid=%04X sid=%04X",
                         indices[i], readers[i].label, request->caid, request->sid);
            return EMU_OK;
        }

        if (!reader_is_internal(&readers[i])) {
            cache_begin = cw_cache_begin_reader(ecm_md5, indices[i], request->cw,
                                                request->account, true, &cache_wait);
            if (cache_begin == CW_CACHE_BEGIN_HIT) {
                if (result) {
                    result->status = EMU_OK;
                    result->reader_index = indices[i];
                    result->cache_hit = 1;
                    int ngrp = readers[i].ngroups;
                    if (ngrp <= 0) {
                        result->groups[0] = 1;
                        result->ngroups = 1;
                    } else {
                        if (ngrp > MAX_GROUPS_PER_READER) ngrp = MAX_GROUPS_PER_READER;
                        memcpy(result->groups, readers[i].groups,
                               (size_t)ngrp * sizeof(result->groups[0]));
                        result->ngroups = ngrp;
                    }
                }
                secure_zero(ecm_md5, sizeof(ecm_md5));
                tcmg_log_dbg(D_READER, "cache hit while registering reader work index=%d label='%s'",
                             indices[i], readers[i].label);
                return EMU_OK;
            }
            if (cache_begin == CW_CACHE_BEGIN_WAIT) {
                if (cw_cache_wait(&cache_wait, ecm_md5, request->cw,
                                  request->account, true)) {
                    if (result) {
                        result->status = EMU_OK;
                        result->reader_index = indices[i];
                        result->cache_hit = 1;
                        int ngrp = readers[i].ngroups;
                        if (ngrp <= 0) {
                            result->groups[0] = 1;
                            result->ngroups = 1;
                        } else {
                            if (ngrp > MAX_GROUPS_PER_READER) ngrp = MAX_GROUPS_PER_READER;
                            memcpy(result->groups, readers[i].groups,
                                   (size_t)ngrp * sizeof(result->groups[0]));
                            result->ngroups = ngrp;
                        }
                    }
                    secure_zero(ecm_md5, sizeof(ecm_md5));
                    tcmg_log_dbg(D_READER, "cw cache pending satisfied reader index=%d label='%s'",
                                 indices[i], readers[i].label);
                    return EMU_OK;
                }

                tcmg_log_dbg(D_READER, "cw cache pending failed reader index=%d label='%s' -> next reader",
                             indices[i], readers[i].label);
                continue;
            }
        }

        if (gate) pthread_mutex_lock(gate);

        if (cw_cache_lookup(ecm_md5, request->cw, request->account, true)) {
            if (cache_begin == CW_CACHE_BEGIN_LEADER)
                cw_cache_complete_reader(ecm_md5, indices[i], true);
            if (result) {
                result->status = EMU_OK;
                result->reader_index = indices[i];
                result->cache_hit = 1;
                int ngrp = readers[i].ngroups;
                if (ngrp <= 0) {
                    result->groups[0] = 1;
                    result->ngroups = 1;
                } else {
                    if (ngrp > MAX_GROUPS_PER_READER) ngrp = MAX_GROUPS_PER_READER;
                    memcpy(result->groups, readers[i].groups,
                           (size_t)ngrp * sizeof(result->groups[0]));
                    result->ngroups = ngrp;
                }
            }
            if (gate) pthread_mutex_unlock(gate);
            secure_zero(ecm_md5, sizeof(ecm_md5));
            return EMU_OK;
        }

        E_READER_FAILURE failure = READER_FAILURE_READER_ERROR;
        S_READER_ECM_REQUEST call = {
            .index = indices[i],
            .reader = &readers[i],
            .request = request,
            .failure = &failure,
        };
        int rc = protocol->do_ecm(&call);
        if (failure_rank(failure) > failure_rank(failure_summary))
            failure_summary = failure;
        reader_stats_record(indices[i], rc == EMU_OK);
        if (rc == EMU_OK) {
            int32_t cache_groups[MAX_GROUPS_PER_READER] = {0};
            int ngrp = readers[i].ngroups;
            if (ngrp <= 0) {
                cache_groups[0] = 1;
                ngrp = 1;
            } else {
                if (ngrp > MAX_GROUPS_PER_READER) ngrp = MAX_GROUPS_PER_READER;
                memcpy(cache_groups, readers[i].groups,
                       (size_t)ngrp * sizeof(cache_groups[0]));
            }
            cw_cache_store_groups(ecm_md5, request->cw, cache_groups, ngrp);
            if (cache_begin == CW_CACHE_BEGIN_LEADER)
                cw_cache_complete_reader(ecm_md5, indices[i], true);
            if (result) {
                result->status = EMU_OK;
                result->reader_index = indices[i];
                result->ngroups = ngrp;
                memcpy(result->groups, cache_groups,
                       (size_t)ngrp * sizeof(result->groups[0]));
            }
            secure_zero(cache_groups, sizeof(cache_groups));
            if (gate) pthread_mutex_unlock(gate);
            secure_zero(ecm_md5, sizeof(ecm_md5));
            return EMU_OK;
        }

        if (cache_begin == CW_CACHE_BEGIN_LEADER)
            cw_cache_complete_reader(ecm_md5, indices[i], false);
        if (gate) pthread_mutex_unlock(gate);
        tcmg_log_dbg(D_READER,
                     "skip/failed index=%d label='%s' protocol=%s caid=%04X sid=%04X rc=%d",
                     indices[i], readers[i].label, readers[i].protocol,
                     request->caid, request->sid, rc);
    }

    int32_t final_result = (!attempted && whitelist_rejected) ? READER_RESULT_REJECTED : READER_RESULT_NOT_FOUND;
    secure_zero(ecm_md5, sizeof(ecm_md5));
    if (result) {
        result->status = final_result;
        result->failure = (!attempted && whitelist_rejected) ? READER_FAILURE_REJECTED :
                          (failure_summary == READER_FAILURE_NONE ? READER_FAILURE_NOT_FOUND : failure_summary);
    }
    return final_result;
}

void reader_shutdown(void)
{
    for (size_t i = 0; i < reader_protocol_count(); i++) {
        const S_READER_PROTOCOL *protocol = reader_protocol_at(i);
        if (protocol && protocol->shutdown) protocol->shutdown();
    }
}
