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

    assert(antishare_check_request(&a, 10, 0x0B00, 100, &delay) == AS_CHECK_OK);
    antishare_record_success(&a, 10, 0x0B00, 100, cw);
    assert(a.as_ecm_count == 1);
    assert(a.as_last_cw_sid == 100);
    assert(a.as_last_cw[0] == 0x11);

    /* A second client wants a different channel: active-channel limit blocks it. */
    assert(antishare_check_request(&a, 11, 0x0B00, 200, &delay) == AS_CHECK_ACTIVE_CHANNELS);

    /* Same client switches channel: with a limit of 1 the old slot is replaced. */
    assert(antishare_check_request(&a, 10, 0x0B00, 200, &delay) == AS_CHECK_OK);
    assert(delay == 1000);
    antishare_record_success(&a, 10, 0x0B00, 200, cw);

    /* Rate limit counts requests regardless of reader/CW success. */
    assert(a.as_ecm_count == 2);
    antishare_record_failure(&a, 10, 0x0B00, 200);
    assert(antishare_check_request(&a, 10, 0x0B00, 200, &delay) == AS_CHECK_OK);
    assert(antishare_check_request(&a, 10, 0x0B00, 200, &delay) == AS_CHECK_ECM_RATE);
    assert(antishare_check_request(&a, 10, 0x0B00, 200, &delay) == AS_CHECK_ECM_RATE);

    tcmg_sleep_ms(2100);
    assert(antishare_check_request(&a, 10, 0x0B00, 200, &delay) == AS_CHECK_OK);

    antishare_release_client(&a, 10);
    pthread_mutex_destroy(&a.as_mtx);
    return 0;
}
