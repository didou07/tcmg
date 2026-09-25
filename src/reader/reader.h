#ifndef TCMG_READER_H_
#define TCMG_READER_H_

#include "core/reader_types.h"
#include "ecm/request.h"
#include "result.h"
#include <stdint.h>

int32_t reader_dispatch_ecm(const S_ECM_REQUEST *request, S_READER_RESULT *result);

void reader_shutdown(void);

#endif
