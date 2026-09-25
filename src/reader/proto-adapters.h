#ifndef TCMG_READER_PROTO_ADAPTERS_H_
#define TCMG_READER_PROTO_ADAPTERS_H_

#include "protocol.h"

int32_t reader_emu_do_ecm(const S_READER_ECM_REQUEST *req);
int32_t reader_pcsc_do_ecm(const S_READER_ECM_REQUEST *req);
int32_t reader_internal_do_ecm(const S_READER_ECM_REQUEST *req);
int32_t reader_cccam_do_ecm(const S_READER_ECM_REQUEST *req);
int32_t reader_newcamd_do_ecm(const S_READER_ECM_REQUEST *req);
int32_t reader_cs378x_do_ecm(const S_READER_ECM_REQUEST *req);

#endif
