#ifndef TCMG_READER_RESULT_H_
#define TCMG_READER_RESULT_H_

#include "core/constants.h"
#include "failure.h"
#include <stdint.h>

#define READER_RESULT_NOT_FOUND (-2)
#define READER_RESULT_REJECTED  (-3)

typedef struct {
    int32_t status;
    int32_t reader_index;
    int8_t  cache_hit;
    E_TCMG_ECM_SOURCE source;
    E_READER_FAILURE failure;
    int32_t groups[MAX_GROUPS_PER_READER];
    int32_t ngroups;
} S_READER_RESULT;

void reader_result_init(S_READER_RESULT *result);

#endif
