#define MODULE_LOG_PREFIX "reader"
#include "reader.h"
#include "core/config_state.h"
#include "ecm/request.h"
#include "account/account.h"
#include "core/utils.h"
#include "log/log.h"
#include "protocol.h"
#include "rules.h"
#include "result.h"
#include "stats.h"
#include "../cache/cw_cache.h"
#include "../crypto/crypto.h"
#include "../security/antishare.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#define READER_WORKER_COUNT MAX_READERS

typedef struct s_reader_dispatch_ctx S_READER_DISPATCH_CTX;
typedef struct s_reader_job S_READER_JOB;

struct s_reader_job {
    S_READER_DISPATCH_CTX *ctx;
    S_READER_JOB *next;
    int index;
    S_READER reader;
    const S_READER_PROTOCOL *protocol;
    uint8_t cw[CW_LEN];
    E_READER_FAILURE failure;
};

struct s_reader_dispatch_ctx {
    S_ACCOUNT *account;
    S_ECM_REQUEST request;
    char user[CFGKEY_LEN];
    uint8_t ecm[255];
    uint8_t ecm_md5[TCMG_ECM_MD5_LEN];
    pthread_mutex_t mtx;
    pthread_cond_t cv;
    _Atomic int refs;
    int active_jobs;
    bool winner;
    bool attempted;
    bool whitelist_rejected;
    E_READER_FAILURE failure_summary;
    S_READER_RESULT result;
    uint8_t cw[CW_LEN];
    int jobs_count;
    S_READER_JOB jobs[MAX_READERS];
};

static pthread_mutex_t s_reader_pool_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_reader_pool_cv = PTHREAD_COND_INITIALIZER;
static S_READER_JOB *s_reader_pool_head;
static S_READER_JOB *s_reader_pool_tail;
static pthread_t s_reader_pool_threads[READER_WORKER_COUNT];
static pthread_once_t s_reader_pool_once = PTHREAD_ONCE_INIT;
static int s_reader_pool_count;
static bool s_reader_pool_stop;

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

static void reader_ctx_retain(S_READER_DISPATCH_CTX *ctx)
{
    if (ctx) atomic_fetch_add_explicit(&ctx->refs, 1, memory_order_relaxed);
}

static void reader_ctx_destroy(S_READER_DISPATCH_CTX *ctx)
{
    if (!ctx) return;
    account_release(ctx->account);
    pthread_cond_destroy(&ctx->cv);
    pthread_mutex_destroy(&ctx->mtx);
    secure_zero(ctx, sizeof(*ctx));
    free(ctx);
}

static void reader_ctx_release(S_READER_DISPATCH_CTX *ctx)
{
    if (!ctx) return;
    if (atomic_fetch_sub_explicit(&ctx->refs, 1, memory_order_acq_rel) == 1)
        reader_ctx_destroy(ctx);
}

static void reader_ctx_record_failure(S_READER_DISPATCH_CTX *ctx, E_READER_FAILURE failure)
{
    if (!ctx) return;
    pthread_mutex_lock(&ctx->mtx);
    if (failure_rank(failure) > failure_rank(ctx->failure_summary))
        ctx->failure_summary = failure;
    pthread_mutex_unlock(&ctx->mtx);
}

static void reader_job_publish_failure(S_READER_JOB *job)
{
    S_READER_DISPATCH_CTX *ctx = job->ctx;
    if (job->failure != READER_FAILURE_NONE)
        reader_ctx_record_failure(ctx, job->failure);
    pthread_mutex_lock(&ctx->mtx);
    if (ctx->active_jobs > 0) ctx->active_jobs--;
    pthread_cond_broadcast(&ctx->cv);
    pthread_mutex_unlock(&ctx->mtx);
}

static void reader_job_run(S_READER_JOB *job)
{
    S_READER_DISPATCH_CTX *ctx;
    S_ECM_REQUEST request;
    int32_t rc;

    if (!job || !job->ctx || !job->protocol || !job->protocol->do_ecm) {
        if (job) reader_job_publish_failure(job);
        return;
    }

    ctx = job->ctx;
    request = ctx->request;
    request.cw = job->cw;
    job->failure = READER_FAILURE_READER_ERROR;
    memset(job->cw, 0, sizeof(job->cw));

    pthread_mutex_lock(&ctx->mtx);
    bool already_done = ctx->winner;
    pthread_mutex_unlock(&ctx->mtx);
    if (already_done) {
        job->failure = READER_FAILURE_NONE;
        reader_job_publish_failure(job);
        return;
    }

    rc = job->protocol->do_ecm(&(S_READER_ECM_REQUEST){
        .index = job->index,
        .reader = &job->reader,
        .request = &request,
        .failure = &job->failure,
    });

    if (job->failure == READER_FAILURE_REJECTED) {
        pthread_mutex_lock(&ctx->mtx);
        ctx->whitelist_rejected = true;
        pthread_mutex_unlock(&ctx->mtx);
    } else {
        reader_stats_record(job->index, rc == EMU_OK);
    }

    if (rc == EMU_OK) {
        int32_t cache_groups[MAX_GROUPS_PER_READER] = {0};
        int ngrp = job->reader.ngroups;
        if (ngrp <= 0) {
            cache_groups[0] = 1;
            ngrp = 1;
        } else {
            if (ngrp > MAX_GROUPS_PER_READER) ngrp = MAX_GROUPS_PER_READER;
            memcpy(cache_groups, job->reader.groups,
                   (size_t)ngrp * sizeof(cache_groups[0]));
        }
        cw_cache_store_groups(ctx->ecm_md5, job->cw, cache_groups, ngrp);
        pthread_mutex_lock(&ctx->mtx);
        if (!ctx->winner) {
            memcpy(ctx->cw, job->cw, sizeof(ctx->cw));
            ctx->result.status = EMU_OK;
            ctx->result.reader_index = job->index;
            ctx->result.cache_hit = 0;
            ctx->result.source = TCMG_ECM_SOURCE_READER;
            ctx->result.failure = READER_FAILURE_NONE;
            ctx->result.ngroups = ngrp;
            memcpy(ctx->result.groups, cache_groups,
                   (size_t)ngrp * sizeof(ctx->result.groups[0]));
            ctx->winner = true;
        }
        if (ctx->active_jobs > 0) ctx->active_jobs--;
        pthread_cond_broadcast(&ctx->cv);
        pthread_mutex_unlock(&ctx->mtx);
        secure_zero(cache_groups, sizeof(cache_groups));
        return;
    }

    tcmg_log_dbg(D_READER,
                 "skip/failed index=%d label='%s' protocol=%s caid=%04X sid=%04X rc=%d",
                 job->index, job->reader.label, job->reader.protocol,
                 request.caid, request.sid, rc);
    reader_job_publish_failure(job);
}

static void reader_job_cancel(S_READER_JOB *job)
{
    if (!job || !job->ctx) return;
    job->failure = READER_FAILURE_NONE;
    reader_job_publish_failure(job);
}

static void *reader_pool_worker(void *arg)
{
    (void)arg;
    for (;;) {
        S_READER_JOB *job;
        pthread_mutex_lock(&s_reader_pool_mtx);
        while (!s_reader_pool_stop && !s_reader_pool_head)
            pthread_cond_wait(&s_reader_pool_cv, &s_reader_pool_mtx);
        if (s_reader_pool_stop) {
            pthread_mutex_unlock(&s_reader_pool_mtx);
            break;
        }
        job = s_reader_pool_head;
        s_reader_pool_head = job->next;
        if (!s_reader_pool_head) s_reader_pool_tail = NULL;
        job->next = NULL;
        pthread_mutex_unlock(&s_reader_pool_mtx);

        reader_job_run(job);
        reader_ctx_release(job->ctx);
    }
    return NULL;
}

static void reader_pool_start_once(void)
{
    for (int i = 0; i < READER_WORKER_COUNT; i++) {
        if (pthread_create(&s_reader_pool_threads[i], NULL, reader_pool_worker, NULL) != 0)
            break;
        s_reader_pool_count++;
    }
}

static bool reader_pool_available(void)
{
    pthread_once(&s_reader_pool_once, reader_pool_start_once);
    return s_reader_pool_count > 0;
}

static bool reader_pool_submit(S_READER_JOB *job)
{
    if (!job) return false;
    pthread_mutex_lock(&s_reader_pool_mtx);
    if (s_reader_pool_stop) {
        pthread_mutex_unlock(&s_reader_pool_mtx);
        return false;
    }
    job->next = NULL;
    if (s_reader_pool_tail)
        s_reader_pool_tail->next = job;
    else
        s_reader_pool_head = job;
    s_reader_pool_tail = job;
    pthread_cond_signal(&s_reader_pool_cv);
    pthread_mutex_unlock(&s_reader_pool_mtx);
    return true;
}

static void reader_pool_shutdown(void)
{
    S_READER_JOB *pending;
    if (s_reader_pool_count == 0) return;
    pthread_mutex_lock(&s_reader_pool_mtx);
    if (s_reader_pool_stop) {
        pthread_mutex_unlock(&s_reader_pool_mtx);
        return;
    }
    s_reader_pool_stop = true;
    pending = s_reader_pool_head;
    s_reader_pool_head = NULL;
    s_reader_pool_tail = NULL;
    pthread_cond_broadcast(&s_reader_pool_cv);
    pthread_mutex_unlock(&s_reader_pool_mtx);

    while (pending) {
        S_READER_JOB *next = pending->next;
        pending->next = NULL;
        reader_job_cancel(pending);
        reader_ctx_release(pending->ctx);
        pending = next;
    }

    for (int i = 0; i < s_reader_pool_count; i++)
        pthread_join(s_reader_pool_threads[i], NULL);
    s_reader_pool_count = 0;
}

static S_READER_DISPATCH_CTX *reader_ctx_create(const S_ECM_REQUEST *request,
                                                 const uint8_t *md5)
{
    S_READER_DISPATCH_CTX *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    if (pthread_mutex_init(&ctx->mtx, NULL) != 0) {
        free(ctx);
        return NULL;
    }
    if (pthread_cond_init(&ctx->cv, NULL) != 0) {
        pthread_mutex_destroy(&ctx->mtx);
        free(ctx);
        return NULL;
    }
    ctx->account = request->account;
    account_retain(ctx->account);
    ctx->request = *request;
    ctx->request.client = NULL;
    ctx->request.account = ctx->account;
    ctx->request.cw = NULL;
    ctx->request.ecm = ctx->ecm;
    ctx->request.ecm_len = request->ecm_len;
    tcmg_strlcpy(ctx->user, request->user ? request->user : "", sizeof(ctx->user));
    ctx->request.user = ctx->user;
    memcpy(ctx->ecm, request->ecm, (size_t)request->ecm_len);
    memcpy(ctx->ecm_md5, md5, sizeof(ctx->ecm_md5));
    reader_result_init(&ctx->result);
    atomic_init(&ctx->refs, 1);
    return ctx;
}

static void reader_fill_jobs(S_READER_DISPATCH_CTX *ctx)
{
    if (!ctx) return;

    pthread_rwlock_rdlock(&g_cfg.acc_lock);
    for (int i = 0; i < MAX_READERS && ctx->jobs_count < MAX_READERS; i++) {
        const S_READER *reader = &g_cfg.readers[i];
        if (!reader->in_use || !reader->enabled) continue;

        E_READER_RULE_RESULT rule = reader_rule_check(reader, ctx->account,
                                                       ctx->request.caid, ctx->request.sid,
                                                       ctx->request.provid, ctx->request.ecm_len);
        if (rule != READER_RULE_ALLOW) {
            if (rule == READER_RULE_ECM_WHITELIST) {
                ctx->whitelist_rejected = true;
                tcmg_log("ecm rejected label='%s' caid=%04X sid=%04X len=%d whitelist=%d",
                         reader->label, ctx->request.caid, ctx->request.sid,
                         ctx->request.ecm_len, reader->ecm_whitelist);
            }
            continue;
        }

        const S_READER_PROTOCOL *protocol = reader_protocol_find(reader->protocol);
        if (!protocol || !protocol->do_ecm) {
            tcmg_log_dbg(D_READER, "index=%d label='%s' unknown protocol=%s",
                         i, reader->label, reader->protocol);
            continue;
        }

        S_READER_JOB *job = &ctx->jobs[ctx->jobs_count++];
        memset(job, 0, sizeof(*job));
        job->ctx = ctx;
        job->index = i;
        job->reader = *reader;
        job->protocol = protocol;
        job->failure = READER_FAILURE_NONE;
        ctx->attempted = true;
    }
    pthread_rwlock_unlock(&g_cfg.acc_lock);
}


int32_t reader_dispatch_ecm(const S_ECM_REQUEST *request, S_READER_RESULT *result)
{
    uint8_t local_md5[TCMG_ECM_MD5_LEN];
    const uint8_t *ecm_md5 = NULL;
    S_READER_DISPATCH_CTX *ctx = NULL;
    S_CW_INFLIGHT_WAIT inflight_wait = { .slot = -1, .generation = 0 };
    bool inflight_leader = false;

    if (result) reader_result_init(result);
    if (!request || !request->account || !request->ecm || !request->cw ||
        request->ecm_len <= 0 || request->ecm_len > 255)
        return -1;

    if (request->ecm_md5_valid) {
        ecm_md5 = request->ecm_md5;
    } else {
        crypt_md5_hash(request->ecm, (size_t)request->ecm_len, local_md5);
        ecm_md5 = local_md5;
    }

    int32_t inflight_groups[MAX_GROUPS_PER_READER] = {0};
    int32_t inflight_ngroups = 0;

    for (;;) {
        inflight_ngroups = 0;
        E_CW_INFLIGHT_BEGIN inflight =
            cw_inflight_begin(ecm_md5, request->cw, request->account, true, &inflight_wait,
                              inflight_groups, &inflight_ngroups);

        if (inflight == CW_INFLIGHT_HIT) {
            if (result) {
                result->status = EMU_OK;
                result->reader_index = -1;
                result->cache_hit = 1;
                result->source = TCMG_ECM_SOURCE_CACHE;
                result->failure = READER_FAILURE_NONE;
                result->ngroups = inflight_ngroups;
                if (inflight_ngroups > 0)
                    memcpy(result->groups, inflight_groups,
                           (size_t)inflight_ngroups * sizeof(result->groups[0]));
            }
            secure_zero(local_md5, sizeof(local_md5));
            return EMU_OK;
        }

        if (inflight == CW_INFLIGHT_WAIT) {
            if (cw_inflight_wait(&inflight_wait, ecm_md5, request->cw, request->account, true,
                                 inflight_groups, &inflight_ngroups)) {
                if (result) {
                    result->status = EMU_OK;
                    result->reader_index = -1;
                    result->cache_hit = 0;
                    result->source = TCMG_ECM_SOURCE_SHARED;
                    result->failure = READER_FAILURE_NONE;
                    result->ngroups = inflight_ngroups;
                    if (inflight_ngroups > 0)
                        memcpy(result->groups, inflight_groups,
                               (size_t)inflight_ngroups * sizeof(result->groups[0]));
                }
                secure_zero(local_md5, sizeof(local_md5));
                return EMU_OK;
            }
            continue;
        }

        inflight_leader = true;
        break;
    }

    ctx = reader_ctx_create(request, ecm_md5);
    if (!ctx) {
        if (inflight_leader) cw_inflight_complete(ecm_md5, false);
        secure_zero(local_md5, sizeof(local_md5));
        return -1;
    }

    reader_fill_jobs(ctx);
    if (ctx->jobs_count == 0) {
        cw_inflight_complete(ecm_md5, false);
        pthread_mutex_lock(&ctx->mtx);
        int32_t final_result = ctx->whitelist_rejected ? READER_RESULT_REJECTED : READER_RESULT_NOT_FOUND;
        if (result) {
            result->status = final_result;
            result->failure = ctx->whitelist_rejected ? READER_FAILURE_REJECTED : READER_FAILURE_NOT_FOUND;
        }
        pthread_mutex_unlock(&ctx->mtx);
        reader_ctx_release(ctx);
        secure_zero(local_md5, sizeof(local_md5));
        return final_result;
    }

    if (antishare_begin_ecm(ctx->account) != AS_CHECK_OK) {
        tcmg_log("ecm rejected: anti-share rate limit user='%s' caid=%04X sid=%04X",
                 ctx->request.user ? ctx->request.user : "?",
                 ctx->request.caid, ctx->request.sid);
        antishare_clear_pending(request->account, request->thread_id, request->caid, request->sid);
        cw_inflight_complete(ecm_md5, false);
        pthread_mutex_lock(&ctx->mtx);
        if (result) {
            result->status = READER_RESULT_REJECTED;
            result->reader_index = -1;
            result->failure = READER_FAILURE_REJECTED;
        }
        pthread_mutex_unlock(&ctx->mtx);
        reader_ctx_release(ctx);
        secure_zero(local_md5, sizeof(local_md5));
        return READER_RESULT_REJECTED;
    }

    if (ctx->jobs_count == 1) {
        ctx->active_jobs = 1;
        reader_job_run(&ctx->jobs[0]);
    } else {
        if (!reader_pool_available()) {
            ctx->active_jobs = ctx->jobs_count;
            for (int i = 0; i < ctx->jobs_count; i++) {
                reader_job_run(&ctx->jobs[i]);
                if (ctx->winner) break;
            }
        } else {
            pthread_mutex_lock(&ctx->mtx);
            ctx->active_jobs = ctx->jobs_count;
            pthread_mutex_unlock(&ctx->mtx);

            for (int i = 0; i < ctx->jobs_count; i++) {
                reader_ctx_retain(ctx);
                if (!reader_pool_submit(&ctx->jobs[i])) {
                    reader_ctx_release(ctx);
                    reader_job_cancel(&ctx->jobs[i]);
                }
            }

            pthread_mutex_lock(&ctx->mtx);
            while (!ctx->winner && ctx->active_jobs > 0)
                pthread_cond_wait(&ctx->cv, &ctx->mtx);
            pthread_mutex_unlock(&ctx->mtx);
        }
    }

    pthread_mutex_lock(&ctx->mtx);
    bool success = ctx->winner;
    if (success) {
        if (result) *result = ctx->result;
        memcpy(request->cw, ctx->cw, CW_LEN);
        pthread_mutex_unlock(&ctx->mtx);
        antishare_end_ecm(ctx->account, true);
        cw_inflight_complete(ecm_md5, success);
        reader_ctx_release(ctx);
        secure_zero(local_md5, sizeof(local_md5));
        return EMU_OK;
    }

    int32_t final_result = (!ctx->attempted && ctx->whitelist_rejected)
                               ? READER_RESULT_REJECTED : READER_RESULT_NOT_FOUND;
    if (result) {
        result->status = final_result;
        result->failure = (!ctx->attempted && ctx->whitelist_rejected)
                              ? READER_FAILURE_REJECTED
                              : (ctx->failure_summary == READER_FAILURE_NONE
                                 ? READER_FAILURE_NOT_FOUND : ctx->failure_summary);
    }
    pthread_mutex_unlock(&ctx->mtx);
    antishare_end_ecm(ctx->account, false);
    cw_inflight_complete(ecm_md5, false);
    reader_ctx_release(ctx);
    secure_zero(local_md5, sizeof(local_md5));
    return final_result;
}


void reader_shutdown(void)
{
    reader_pool_shutdown();
    for (size_t i = 0; i < reader_protocol_count(); i++) {
        const S_READER_PROTOCOL *protocol = reader_protocol_at(i);
        if (protocol && protocol->shutdown) protocol->shutdown();
    }
}
