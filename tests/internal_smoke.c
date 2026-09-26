#include "core/config_state.h"
#include "internal/internal.h"
#include "reader/protocol.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    const S_READER_PROTOCOL *p = reader_protocol_find("internal");
    if (!p || !p->do_ecm) {
        fprintf(stderr, "FAIL: internal protocol registry\n");
        return 1;
    }

    memset(&g_cfg, 0, sizeof(g_cfg));
    if (pthread_rwlock_init(&g_cfg.acc_lock, NULL) != 0) {
        fprintf(stderr, "FAIL: config lock init\n");
        return 2;
    }
    g_cfg.readers[0].in_use = 1;
    g_cfg.readers[0].enabled = 1;
    strcpy(g_cfg.readers[0].protocol, "internal");
    strcpy(g_cfg.readers[0].device, "/dev/tcmg-no-such-sci");
    g_cfg.readers[0].fast_reset = 60;
    g_cfg.readers[0].poll_ms = 50;

    if (internal_start() != 0) {
        fprintf(stderr, "FAIL: internal_start\n");
        pthread_rwlock_destroy(&g_cfg.acc_lock);
        return 3;
    }
    usleep(100000);

    S_INTERNAL_READER r;
    if (internal_reader_get(-1, &r) != -1 || internal_reader_get(0, &r) != -1) {
        fprintf(stderr, "FAIL: internal_reader_get state boundary\n");
        internal_stop();
        pthread_rwlock_destroy(&g_cfg.acc_lock);
        return 4;
    }
    if (internal_reader_count() != 0) {
        fprintf(stderr, "FAIL: invalid device became an active reader\n");
        internal_stop();
        pthread_rwlock_destroy(&g_cfg.acc_lock);
        return 5;
    }

    internal_stop();
    pthread_rwlock_destroy(&g_cfg.acc_lock);
    puts("INTERNAL: PASS");
    return 0;
}
