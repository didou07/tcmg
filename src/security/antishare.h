#ifndef TCMG_ANTISHARE_H_
#define TCMG_ANTISHARE_H_

#include "../core/account_types.h"
#include <stdint.h>

typedef enum {
    AS_CHECK_OK = 0,
    AS_CHECK_ECM_RATE,
    AS_CHECK_ACTIVE_CHANNELS
} T_ANTISHARE_STATUS;

T_ANTISHARE_STATUS antishare_check_request(S_ACCOUNT *acc, uint32_t client_tid, uint16_t caid, uint16_t sid, int *delay_ms);
void antishare_record_success(S_ACCOUNT *acc, uint32_t client_tid, uint16_t caid, uint16_t sid, const uint8_t cw[CW_LEN]);
void antishare_record_failure(S_ACCOUNT *acc, uint32_t client_tid, uint16_t caid, uint16_t sid);
void antishare_release_client(S_ACCOUNT *acc, uint32_t client_tid);

#endif
