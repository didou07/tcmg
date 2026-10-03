#include "log/log.h"
#include "core/utils.h"
#include "core/constants.h"
#include <assert.h>
#include <pthread.h>
#include <string.h>

typedef struct {
    int duplicates;
    int found;
    int cache;
    int shared;
} LOG_COUNTS;

static void *thread_log(void *arg)
{
    (void)arg;
    log_set_type(LOG_TYPE_CLIENT);
    log_set_user("tester");
    tcmg_log_txt("test", "%s", "thread-duplicate");
    return NULL;
}

static int count_text_cb(int32_t id, const char *line, const char *usr, void *ctx)
{
    LOG_COUNTS *counts = (LOG_COUNTS *)ctx;
    (void)id;
    (void)usr;
    if (strstr(line, "thread-duplicate")) counts->duplicates++;
    if (strstr(line, ": found (")) counts->found++;
    if (strstr(line, ": cache (")) counts->cache++;
    if (strstr(line, ": shared (")) counts->shared++;
    return 0;
}

int main(void)
{
    uint8_t cw[CW_LEN] = {0};
    LOG_COUNTS counts = {0};
    pthread_t tid;
    log_init();
    log_set_type(LOG_TYPE_CLIENT);
    log_set_user("tester");
    tcmg_log_txt("test", "%s", "same-line");
    tcmg_sleep_ms(1100);
    tcmg_log_txt("test", "%s", "same-line");
    tcmg_log_force_txt("test", "%s", "force-line");
    tcmg_log_txt("test", "%s", "same-line");

    cw[0] = 0x11;
    log_cw_result(0x0B00, 0x04C2, 7, cw, LOG_ECM_FOUND, TCMG_ECM_SOURCE_READER, 12, "tester");
    log_cw_result(0x0B00, 0x04C2, 7, cw, LOG_ECM_FOUND, TCMG_ECM_SOURCE_CACHE, 0, "tester");
    log_cw_result(0x0B00, 0x04C2, 7, cw, LOG_ECM_FOUND, TCMG_ECM_SOURCE_SHARED, 3, "tester");
    log_cw_result(0x0B00, 0x04C2, 7, cw, LOG_ECM_REJECTED, TCMG_ECM_SOURCE_READER, 0, "tester");
    log_cw_result(0x0B00, 0x04C2, 7, cw, LOG_ECM_REJECTED, TCMG_ECM_SOURCE_READER, 0, "tester");

    tcmg_log_txt("test", "%s", "thread-duplicate");
    assert(pthread_create(&tid, NULL, thread_log, NULL) == 0);
    assert(pthread_join(tid, NULL) == 0);
    log_flush();
    assert(log_ring_total() == 10);
    log_ring_foreach(0, 100, count_text_cb, &counts, NULL);
    assert(counts.duplicates == 1);
    assert(counts.found == 1);
    assert(counts.cache == 1);
    assert(counts.shared == 1);
    log_shutdown();
    return 0;
}
