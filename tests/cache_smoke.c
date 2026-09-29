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
    S_CW_CACHE_WAIT wait = { .slot = -1, .generation = 0 };
    E_CW_CACHE_BEGIN begin =
        cw_cache_begin_reader(ctx->md5, 3, out, &ctx->account, true, &wait);

    if (begin != CW_CACHE_BEGIN_WAIT) {
        ctx->result = -1;
        atomic_fetch_add(ctx->joined, 1);
        return NULL;
    }

    atomic_fetch_add(ctx->joined, 1);
    ctx->result = cw_cache_wait(&wait, ctx->md5, out, &ctx->account, true) &&
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
    one.ngroups = 0;
    two.group = 8;
    two.ngroups = 0;

    cw_cache_store_groups(md5, cw, groups, 1);

    memset(out, 0, sizeof(out));
    assert(cw_cache_lookup(md5, out, &one, true));
    assert(memcmp(out, cw, CW_LEN) == 0);

    memset(out, 0, sizeof(out));
    assert(!cw_cache_lookup(md5, out, &two, true));

    memset(out, 0, sizeof(out));
    assert(cw_cache_lookup(md5, out, &two, false));
    assert(memcmp(out, cw, CW_LEN) == 0);

    /* Same ECM + same CW merges groups instead of replacing the old scope. */
    int32_t group8[] = { 8 };
    cw_cache_store_groups(md5, cw, group8, 1);
    memset(out, 0, sizeof(out));
    assert(cw_cache_lookup(md5, out, &two, true));
    assert(memcmp(out, cw, CW_LEN) == 0);

    /* Different CWs for the same ECM may coexist; the more frequently
       observed candidate remains preferred for an eligible group. */
    uint8_t cw2[CW_LEN];
    for (int i = 0; i < CW_LEN; i++) cw2[i] = (uint8_t)(0xE0 + i);
    cw_cache_store_groups(md5, cw2, groups, 1);
    memset(out, 0, sizeof(out));
    assert(cw_cache_lookup(md5, out, &one, true));
    assert(memcmp(out, cw, CW_LEN) == 0);

    /* Concurrent same-ECM test: one leader, all other requests wait for it. */
    for (int i = 0; i < 16; i++) md5[i] = (uint8_t)(0x40 + i);
    for (int i = 0; i < CW_LEN; i++) cw[i] = (uint8_t)(0x10 + i);

    assert(!cw_cache_lookup(md5, out, &one, true));
    S_CW_CACHE_WAIT leader_wait = { .slot = -1, .generation = 0, .reader_index = 3 };
    assert(cw_cache_begin_reader(md5, 3, out, &one, true, &leader_wait) == CW_CACHE_BEGIN_LEADER);

    S_CW_CACHE_WAIT other_reader = { .slot = -1, .generation = 0, .reader_index = 4 };
    assert(cw_cache_begin_reader(md5, 4, out, &one, true, &other_reader) == CW_CACHE_BEGIN_LEADER);
    cw_cache_complete_reader(md5, 4, false);

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
    cw_cache_complete_reader(md5, 3, true);

    for (int i = 0; i < WAITER_COUNT; i++) {
        assert(pthread_join(threads[i], NULL) == 0);
        assert(ctx[i].result == 1);
    }

    memset(out, 0, sizeof(out));
    assert(cw_cache_lookup(md5, out, &one, true));
    assert(memcmp(out, cw, CW_LEN) == 0);

    /* Failed leader must wake waiters, and the next request can become leader. */
    for (int i = 0; i < 16; i++) md5[i] = (uint8_t)(0x80 + i);
    assert(cw_cache_begin_reader(md5, 3, out, &one, true, &leader_wait) == CW_CACHE_BEGIN_LEADER);

    waiter_ctx_t failed_ctx;
    memset(&failed_ctx, 0, sizeof(failed_ctx));
    memcpy(failed_ctx.md5, md5, sizeof(md5));
    memcpy(failed_ctx.expected_cw, cw, sizeof(cw));
    failed_ctx.account.group = 7;
    atomic_int failed_joined = 0;
    failed_ctx.joined = &failed_joined;

    pthread_t failed_thread;
    assert(pthread_create(&failed_thread, NULL, waiter_thread, &failed_ctx) == 0);
    while (atomic_load(&failed_joined) != 1)
        tcmg_sleep_ms(1);
    cw_cache_complete_reader(md5, 3, false);
    assert(pthread_join(failed_thread, NULL) == 0);
    assert(failed_ctx.result == 0);

    memset(out, 0, sizeof(out));
    assert(!cw_cache_lookup(md5, out, &one, true));
    assert(cw_cache_begin_reader(md5, 3, out, &one, true, &leader_wait) == CW_CACHE_BEGIN_LEADER);
    cw_cache_complete_reader(md5, 3, false);

    puts("cache_smoke: PASS");
    return 0;
}
