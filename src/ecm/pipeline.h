#ifndef TCMG_ECM_PIPELINE_H_
#define TCMG_ECM_PIPELINE_H_

#include "types.h"

int32_t ecm_process(S_CLIENT *client, uint16_t caid, uint16_t sid, uint32_t provid,
                    const uint8_t *ecm, int32_t ecm_len, uint8_t cw[CW_LEN],
                    S_ECM_RESULT *result);

#endif
