#ifndef TCMG_ECM_REQUEST_H_
#define TCMG_ECM_REQUEST_H_

#include "types.h"

void ecm_request_init(S_ECM_REQUEST *request, S_CLIENT *client,
                      uint16_t caid, uint16_t sid, uint32_t provid,
                      const uint8_t *ecm, int32_t ecm_len,
                      uint8_t *cw);

#endif
