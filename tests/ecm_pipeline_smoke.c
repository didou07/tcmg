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
    account.group = 1;
    account.sched_day_from = -1;
    account.sched_day_to = -1;
    assert(account_stats_init(&account.stats));

    memset(&client, 0, sizeof(client));
    client_init(&client, 10, "127.0.0.1");
    client.auth.account = &account;

    assert(ecm_access(&client, 0x0B00, 0x0064, 0, false, true, false) == ECM_ACCESS_OK);
    assert(ecm_access(&client, 0x0500, 0x0064, 0, false, true, false) == ECM_ACCESS_CAID_DENIED);
    account.caid = 0;
    assert(ecm_access(&client, 0x0500, 0x0064, 0, false, true, false) == ECM_ACCESS_OK);
    account.caid = 0x0B00;

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
    assert(ecm_process(&client, 0x0B00, 0x0064, 0, ecm, sizeof(ecm), cw, &result) == EMU_OK);
    assert(result.cache_hit);
    assert(memcmp(cw, cached_cw, CW_LEN) == 0);

    ecm[0] ^= 0x01;

    memset(&g_cfg.readers[0], 0, sizeof(g_cfg.readers[0]));
    g_cfg.readers[0].in_use = true;
    g_cfg.readers[0].enabled = true;
    snprintf(g_cfg.readers[0].label, sizeof(g_cfg.readers[0].label), "test");
    snprintf(g_cfg.readers[0].protocol, sizeof(g_cfg.readers[0].protocol), "emu");
    g_cfg.readers[0].ngroups = 1;
    g_cfg.readers[0].groups[0] = 1;
    g_cfg.readers[0].ecm_whitelist = 0x37;

    S_ACCOUNT_STATS_SNAPSHOT before_rejected;
    account_stats_snapshot(&account, &before_rejected);
    S_ECM_RESULT rejected;
    uint8_t rejected_cw[CW_LEN] = {0};
    assert(ecm_process(&client, 0x0B00, 0x0064, 0, ecm, sizeof(ecm), rejected_cw, &rejected) == READER_RESULT_REJECTED);
    S_ACCOUNT_STATS_SNAPSHOT rejected_stats;
    account_stats_snapshot(&account, &rejected_stats);
    assert(rejected_stats.cw_not == before_rejected.cw_not);
    assert(rejected_stats.ecm_total == before_rejected.ecm_total + 1);

    pthread_rwlock_destroy(&g_cfg.acc_lock);

    pthread_mutex_destroy(&account.stats.lock);
    puts("ecm_pipeline_smoke: PASS");
    return 0;
}
