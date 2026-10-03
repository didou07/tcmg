#include "cache/cw_cache.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

#define WAITER_COUNT 8

typedef struct {
    uint8_t md5[16];
    uint8_t expected_cw[CW_LEN];
    S_ACCOUNT account;
    atomic_int *joined;
    int result;
} waiter_ctx_t;

static void *waiter_thread(void *arg)
{
    waiter_ctx_t *ctx = (waiter_ctx_t *)arg;
    uint8_t out[CW_LEN] = {0};
    S_CW_INFLIGHT_WAIT wait = { .slot = -1, .generation = 0 };
    E_CW_INFLIGHT_BEGIN begin =
        cw_inflight_begin(ctx->md5, out, &ctx->account, true, &wait, NULL, NULL);

    if (begin != CW_INFLIGHT_WAIT) {
        ctx->result = -1;
        atomic_fetch_add(ctx->joined, 1);
        return NULL;
    }

    atomic_fetch_add(ctx->joined, 1);
    ctx->result = cw_inflight_wait(&wait, ctx->md5, out, &ctx->account, true, NULL, NULL) &&
                  memcmp(out, ctx->expected_cw, CW_LEN) == 0;
    return NULL;
}

int main(void)
{
    S_ACCOUNT one;
    S_ACCOUNT two;
    uint8_t md5[16];
    uint8_t cw[CW_LEN];
    uint8_t out[CW_LEN];
    int32_t groups[] = { 7 };

    memset(&one, 0, sizeof(one));
    memset(&two, 0, sizeof(two));
    for (int i = 0; i < 16; i++) md5[i] = (uint8_t)i;
    for (int i = 0; i < CW_LEN; i++) cw[i] = (uint8_t)(0xA0 + i);
    one.group = 7;
    two.group = 8;

    cw_cache_store_groups(md5, cw, groups, 1);
    assert(cw_cache_lookup(md5, out, &one, true));
    assert(memcmp(out, cw, CW_LEN) == 0);
    assert(!cw_cache_lookup(md5, out, &two, true));

    S_CW_INFLIGHT_WAIT hit_wait = { .slot = -1, .generation = 0 };
    assert(cw_inflight_begin(md5, out, &one, true, &hit_wait, NULL, NULL) == CW_INFLIGHT_HIT);
    assert(memcmp(out, cw, CW_LEN) == 0);

    for (int i = 0; i < 16; i++) md5[i] = (uint8_t)(0x40 + i);
    for (int i = 0; i < CW_LEN; i++) cw[i] = (uint8_t)(0x10 + i);
    assert(!cw_cache_lookup(md5, out, &one, true));

    S_CW_INFLIGHT_WAIT leader = { .slot = -1, .generation = 0 };
    assert(cw_inflight_begin(md5, out, &one, true, &leader, NULL, NULL) == CW_INFLIGHT_LEADER);

    pthread_t threads[WAITER_COUNT];
    waiter_ctx_t ctx[WAITER_COUNT];
    atomic_int joined = 0;
    for (int i = 0; i < WAITER_COUNT; i++) {
        memset(&ctx[i], 0, sizeof(ctx[i]));
        memcpy(ctx[i].md5, md5, sizeof(md5));
        memcpy(ctx[i].expected_cw, cw, sizeof(cw));
        ctx[i].account.group = 7;
        ctx[i].joined = &joined;
        assert(pthread_create(&threads[i], NULL, waiter_thread, &ctx[i]) == 0);
    }

    while (atomic_load(&joined) != WAITER_COUNT)
        tcmg_sleep_ms(1);

    int32_t pending_groups[] = { 7 };
    cw_cache_store_groups(md5, cw, pending_groups, 1);
    cw_inflight_complete(md5, true);

    for (int i = 0; i < WAITER_COUNT; i++) {
        assert(pthread_join(threads[i], NULL) == 0);
        assert(ctx[i].result == 1);
    }

    for (int i = 0; i < 16; i++) md5[i] = (uint8_t)(0x80 + i);
    assert(cw_inflight_begin(md5, out, &one, true, &leader, NULL, NULL) == CW_INFLIGHT_LEADER);
    waiter_ctx_t failed;
    memset(&failed, 0, sizeof(failed));
    memcpy(failed.md5, md5, sizeof(md5));
    failed.account.group = 7;
    atomic_int failed_joined = 0;
    failed.joined = &failed_joined;
    pthread_t failed_thread;
    assert(pthread_create(&failed_thread, NULL, waiter_thread, &failed) == 0);
    while (atomic_load(&failed_joined) != 1)
        tcmg_sleep_ms(1);
    cw_inflight_complete(md5, false);
    assert(pthread_join(failed_thread, NULL) == 0);
    assert(failed.result == 0);

    puts("cache_smoke: PASS");
    return 0;
}
