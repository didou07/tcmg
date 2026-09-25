#ifndef TCMG_READER_RESULT_H_
#define TCMG_READER_RESULT_H_

#include "core/constants.h"
#include <stdint.h>

typedef struct {
    int32_t status;
    int32_t reader_index;
    int32_t groups[MAX_GROUPS_PER_READER];
    int32_t ngroups;
} S_READER_RESULT;

void reader_result_init(S_READER_RESULT *result);

#endif
