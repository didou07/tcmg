#ifndef TCMG_ANTISHARE_H_
#define TCMG_ANTISHARE_H_

#include "../core/account_types.h"
#include <stdint.h>

#define TCMG_ANTISHARE_ECM_MD5_LEN 16

typedef enum {
    AS_CHECK_OK = 0,
    AS_CHECK_RATE_LIMIT,
    AS_CHECK_CHANNEL_LIMIT
} T_ANTISHARE_STATUS;

T_ANTISHARE_STATUS antishare_check_channel(S_ACCOUNT *acc, uint32_t client_tid,
                                            uint16_t caid, uint16_t sid, int *delay_ms);
T_ANTISHARE_STATUS antishare_begin_ecm(S_ACCOUNT *acc);
void antishare_end_ecm(S_ACCOUNT *acc, bool success);
void antishare_record_channel_success(S_ACCOUNT *acc, uint32_t client_tid,
                                       uint16_t caid, uint16_t sid, const uint8_t cw[CW_LEN]);
void antishare_clear_pending(S_ACCOUNT *acc, uint32_t client_tid, uint16_t caid, uint16_t sid);
void antishare_release_client(S_ACCOUNT *acc, uint32_t client_tid);
void antishare_copy_runtime(S_ACCOUNT *dst, const S_ACCOUNT *src);

#endif
