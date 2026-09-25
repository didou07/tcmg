#ifndef TCMG_CACHE_TYPES_H_
#define TCMG_CACHE_TYPES_H_

#include "../core/compat.h"
#include "../core/constants.h"

typedef struct {
    uint8_t ecm_md5[16];
    uint8_t cw[CW_LEN];
    time_t  ts;
    int8_t  valid;
    int8_t  scoped;
    int32_t groups[MAX_GROUPS_PER_READER];
    int32_t ngroups;
} S_CW_CACHE_ENTRY;

#endif
