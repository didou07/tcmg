#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "../src/security/antishare.h"
#include "../src/core/account_types.h"
#include "../src/core/compat.h"

int main(void)
{
    S_ACCOUNT a;
    memset(&a, 0, sizeof(a));
    a.anti_share = 1;
    a.as_max_sids = 1;
    a.as_max_ecm = 3;
    a.as_ecm_window_s = 2;
    a.as_channel_timeout_s = 2;
    a.as_switch_delay_s = 1;
    pthread_mutex_init(&a.as_mtx, NULL);
    uint8_t cw[CW_LEN]; memset(cw, 0x11, sizeof(cw));
    int delay = 0;

    assert(antishare_check_channel(&a, 10, 0x0B00, 100, &delay) == AS_CHECK_OK);
    assert(antishare_begin_ecm(&a) == AS_CHECK_OK);
    antishare_end_ecm(&a, true);
    antishare_record_channel_success(&a, 10, 0x0B00, 100, cw);
    assert(a.as_ecm_points == 1);
    assert(a.as_last_cw_sid == 100);
    assert(a.as_last_cw[0] == 0x11);

    /* A second client on the same CAID/SID is still one active channel.
     * This models cache/shared delivery without bypassing anti-share. */
    assert(antishare_check_channel(&a, 11, 0x0B00, 100, &delay) == AS_CHECK_OK);
    assert(a.as_channel_count == 1);
    assert(antishare_check_channel(&a, 12, 0x0B00, 200, &delay) == AS_CHECK_CHANNEL_LIMIT);

    /* Do not allow one client to switch away from a SID that another client
     * is still using when max_sids has already been reached. */
    assert(antishare_check_channel(&a, 10, 0x0B00, 200, &delay) == AS_CHECK_CHANNEL_LIMIT);

    /* Once the other client releases SID 100, client 10 may switch to SID 200. */
    antishare_release_client(&a, 11);
    assert(a.as_channel_count == 1);
    assert(antishare_check_channel(&a, 10, 0x0B00, 200, &delay) == AS_CHECK_OK);
    assert(a.as_channel_count == 1);
    assert(delay == 0);
    assert(antishare_begin_ecm(&a) == AS_CHECK_OK);
    antishare_end_ecm(&a, true);
    antishare_record_channel_success(&a, 10, 0x0B00, 200, cw);
    assert(a.as_channel_count == 1);

    /* Increasing the limit allows a second distinct SID. Repeated cache/shared
     * requests on the same SID still remain one active channel. */
    a.as_max_sids = 2;
    assert(antishare_check_channel(&a, 11, 0x0B00, 200, &delay) == AS_CHECK_OK);
    assert(a.as_channel_count == 1);
    assert(antishare_check_channel(&a, 12, 0x0B00, 200, &delay) == AS_CHECK_OK);
    assert(a.as_channel_count == 1);
    assert(antishare_check_channel(&a, 12, 0x0B00, 100, &delay) == AS_CHECK_OK);
    assert(a.as_channel_count == 2);

    /* One quick switch alone is tolerated. Repeated recent channels or a
     * three-channel burst may arm the soft CW delay without blocking access. */
    a.as_max_sids = 32;
    a.as_channel_timeout_s = 15;
    memset(a.as_recent_channels, 0, sizeof(a.as_recent_channels));
    a.as_recent_count = 0;
    memset(a.as_channels, 0, sizeof(a.as_channels));
    a.as_channel_count = 0;
    a.as_suspicion_score = 0;
    a.as_suspicion_last_ms = 0;
    delay = 0;
    assert(antishare_check_channel(&a, 20, 0x0B00, 300, &delay) == AS_CHECK_OK);
    assert(delay == 0);
    assert(antishare_check_channel(&a, 20, 0x0B00, 301, &delay) == AS_CHECK_OK);
    assert(delay == 0);
    assert(antishare_check_channel(&a, 20, 0x0B00, 302, &delay) == AS_CHECK_OK);
    assert(delay == 1000);

    assert(antishare_check_channel(&a, 20, 0x0B00, 300, &delay) == AS_CHECK_OK);
    assert(antishare_check_channel(&a, 20, 0x0B00, 301, &delay) == AS_CHECK_OK);
    assert(delay == 1000);

    assert(a.as_ecm_points == 2);
    assert(antishare_check_channel(&a, 10, 0x0B00, 200, &delay) == AS_CHECK_OK);
    assert(antishare_begin_ecm(&a) == AS_CHECK_OK);
    antishare_end_ecm(&a, false);
    assert(a.as_ecm_points == 2);
    /* ECM rate points remain separate from active-channel accounting.
     * cache/shared are already covered by antishare_check_channel(). */
    assert(antishare_begin_ecm(&a) == AS_CHECK_OK);
    antishare_end_ecm(&a, true);
    assert(a.as_ecm_points == 3);
    assert(antishare_begin_ecm(&a) == AS_CHECK_RATE_LIMIT);

    tcmg_sleep_ms(2100);
    assert(antishare_check_channel(&a, 10, 0x0B00, 200, &delay) == AS_CHECK_OK);


    S_ACCOUNT b;
    memset(&b, 0, sizeof(b));
    b.anti_share = 1;
    b.as_max_sids = 2;
    b.as_ecm_window_s = 60;
    b.as_channel_timeout_s = 15;
    b.as_switch_delay_s = 1;
    pthread_mutex_init(&b.as_mtx, NULL);
    antishare_copy_runtime(&b, &a);
    assert(b.as_channel_count == a.as_channel_count);
    assert(b.as_recent_count == a.as_recent_count);
    assert(b.as_ecm_points == a.as_ecm_points);
    assert(b.as_ecm_active == 0);
    assert(b.as_suspicion_score == a.as_suspicion_score);
    pthread_mutex_destroy(&b.as_mtx);

    antishare_release_client(&a, 10);
    pthread_mutex_destroy(&a.as_mtx);
    return 0;
}
