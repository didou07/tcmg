#include "core/config_state.h"
#include "config/runtime_access.h"
#include "config/config.h"
#include <stdio.h>
#include <string.h>

static void init_cfg(void)
{
    memset(&g_cfg, 0, sizeof(g_cfg));
    pthread_rwlock_init(&g_cfg.acc_lock, NULL);
    pthread_mutex_init(&g_cfg.ban_lock, NULL);
    g_cfg.failban_enabled = 1;
    g_cfg.failban_max_fails = 7;
    g_cfg.failban_ban_secs = 600;
    snprintf(g_cfg.failban_allowlist, sizeof(g_cfg.failban_allowlist), "127.0.0.1,10.0.0.0/8");
    g_cfg.readers[3].in_use = 1;
    g_cfg.readers[3].enabled = 1;
    snprintf(g_cfg.readers[3].protocol, sizeof(g_cfg.readers[3].protocol), "pcsc");
    snprintf(g_cfg.readers[3].device, sizeof(g_cfg.readers[3].device), "Reader 3");
    g_cfg.readers[3].fast_reset = 60;
    g_cfg.readers[3].poll_ms = 400;
}

int main(void)
{
    S_CONFIG_FAILBAN_VIEW ban;
    S_READER readers[2];
    int n;

    init_cfg();
    if (!cfg_runtime_failban_snapshot(&ban) || !ban.enabled ||
        ban.max_fails != 7 || ban.ban_secs != 600 ||
        strcmp(ban.allowlist, "127.0.0.1,10.0.0.0/8") != 0) return 2;
    n = cfg_runtime_reader_snapshot(readers, 2);
    if (n != 1 || readers[0].enabled != 1 ||
        strcmp(readers[0].protocol, "pcsc") != 0 ||
        strcmp(readers[0].device, "Reader 3") != 0 ||
        readers[0].fast_reset != 60 || readers[0].poll_ms != 400) return 3;
    if (cfg_runtime_failban_snapshot(NULL) || cfg_runtime_reader_snapshot(NULL, 1) != 0) return 4;

    cfg_accounts_free(&g_cfg);
    pthread_rwlock_destroy(&g_cfg.acc_lock);
    pthread_mutex_destroy(&g_cfg.ban_lock);
    printf("CONFIG_RUNTIME_ACCESS: PASS\n");
    return 0;
}
