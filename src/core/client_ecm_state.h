#ifndef TCMG_CLIENT_ECM_STATE_H_
#define TCMG_CLIENT_ECM_STATE_H_

#include "constants.h"
#include <stdatomic.h>
#include <stdint.h>
#include <time.h>

typedef struct {
    uint16_t caid;
    uint16_t last_caid;
    uint16_t last_srvid;
    char last_channel[SRVID_NAME_MAX];
    _Atomic time_t last_ecm_time;
    int32_t antishare_delay_ms;
    uint8_t antishare_prepared;
} S_CLIENT_ECM_STATE;

#endif
