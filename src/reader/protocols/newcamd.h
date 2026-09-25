#ifndef TCMG_READER_PROTOCOL_NEWCAMD_H_
#define TCMG_READER_PROTOCOL_NEWCAMD_H_

#include "core/reader_types.h"
#include "core/constants.h"

int32_t newcamd_reader_do_ecm(int index, const S_READER *reader,
                              uint16_t caid, uint16_t sid, uint32_t provid,
                              const uint8_t *ecm, int32_t ecm_len,
                              uint8_t cw[CW_LEN]);
void newcamd_reader_shutdown(void);

#endif
