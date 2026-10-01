#include "log/log.h"
#include "core/utils.h"
#include "core/constants.h"
#include <assert.h>
#include <pthread.h>
#include <string.h>

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
    int *count = (int *)ctx;
    (void)id;
    (void)usr;
    if (strstr(line, "thread-duplicate")) (*count)++;
    return 0;
}

int main(void)
{
    uint8_t cw[CW_LEN] = {0};
    int count = 0;
    pthread_t tid;
    log_init();
    log_set_type(LOG_TYPE_CLIENT);
    log_set_user("tester");
    tcmg_log_txt("test", "%s", "same-line");
    tcmg_sleep_ms(1100);
    tcmg_log_txt("test", "%s", "same-line");
    tcmg_log_force_txt("test", "%s", "force-line");
    tcmg_log_txt("test", "%s", "same-line");
    log_cw_result(0x0B00, 0x04C2, 7, cw, LOG_ECM_REJECTED, false, 0, "tester");
    log_cw_result(0x0B00, 0x04C2, 7, cw, LOG_ECM_REJECTED, false, 0, "tester");
    tcmg_log_txt("test", "%s", "thread-duplicate");
    assert(pthread_create(&tid, NULL, thread_log, NULL) == 0);
    assert(pthread_join(tid, NULL) == 0);
    log_flush();
    assert(log_ring_total() == 7);
    log_ring_foreach(0, 100, count_text_cb, &count, NULL);
    assert(count == 1);
    log_shutdown();
    return 0;
}
