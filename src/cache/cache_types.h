#ifndef TCMG_CACHE_TYPES_H_
#define TCMG_CACHE_TYPES_H_

#include "../core/compat.h"
#include "../core/constants.h"

typedef struct {
    uint8_t ecm_md5[16];
    uint8_t cw[CW_LEN];
    int64_t ts_ms;
    uint64_t last_used_ms;
    uint32_t seq;
    uint32_t count;
    int8_t  valid;
    int8_t  scoped;
    int32_t groups[MAX_GROUPS_PER_READER];
    int32_t ngroups;
} S_CW_CACHE_ENTRY;

#endif
