#include "ecm/ecm.h"
#include "core/config_state.h"
#include "client/client.h"
#include "stats/account_stats.h"
#include "cache/cw_cache.h"
#include "crypto/crypto.h"
#include "reader/result.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

int main(void)
{
    S_ACCOUNT account;
    S_CLIENT client;
    uint8_t ecm[16] = {0};
    uint8_t cw[CW_LEN] = {0};
    S_ECM_RESULT result;

    memset(&account, 0, sizeof(account));
    account.enabled = 1;
    account.max_connections = 10;
    account.caid = 0x0B00;
    account.sched_day_from = -1;
    account.sched_day_to = -1;
    assert(account_stats_init(&account.stats));

    memset(&client, 0, sizeof(client));
    client_init(&client, 10, "127.0.0.1");
    client.auth.account = &account;

    assert(ecm_access(&client, 0x0B00, 0x0064, false, true, false) == ECM_ACCESS_OK);
    assert(ecm_access(&client, 0x0500, 0x0064, false, true, false) == ECM_ACCESS_CAID_DENIED);

    assert(ecm_process(NULL, 0x0B00, 0x0064, 0, ecm, sizeof(ecm), cw, &result) < 0);

    memset(&g_cfg, 0, sizeof(g_cfg));
    assert(pthread_rwlock_init(&g_cfg.acc_lock, NULL) == 0);
    uint8_t md5[TCMG_ECM_MD5_LEN];
    crypt_md5_hash(ecm, sizeof(ecm), md5);
    uint8_t cached_cw[CW_LEN];
    for (int i = 0; i < CW_LEN; i++) cached_cw[i] = (uint8_t)(0x40 + i);
    int32_t groups[] = {1};
    cw_cache_store_groups(md5, cached_cw, groups, 1);
    client.auth.account = &account;
    memset(cw, 0, sizeof(cw));
    assert(ecm_process(&client, 0x0B00, 0x0064, 0, ecm, sizeof(ecm), cw, &result) == READER_RESULT_NOT_FOUND);
    assert(!result.cache_hit);
    pthread_rwlock_destroy(&g_cfg.acc_lock);

    pthread_mutex_destroy(&account.stats.lock);
    puts("ecm_pipeline_smoke: PASS");
    return 0;
}
