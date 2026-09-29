#ifndef TCMG_CW_CACHE_H_
#define TCMG_CW_CACHE_H_

#include "cache_types.h"
#include "../core/account_types.h"
#include "../core/reader_types.h"
#include <stdbool.h>
#include <stdint.h>

bool cw_cache_lookup(const uint8_t *ecm_md5, uint8_t *cw_out,
                     const S_ACCOUNT *acc, bool require_group);

typedef enum {
    CW_CACHE_BEGIN_HIT = 0,
    CW_CACHE_BEGIN_LEADER = 1,
    CW_CACHE_BEGIN_WAIT = 2
} E_CW_CACHE_BEGIN;

typedef struct {
    int32_t slot;
    uint32_t generation;
    int32_t reader_index;
} S_CW_CACHE_WAIT;

E_CW_CACHE_BEGIN cw_cache_begin_reader(const uint8_t *ecm_md5, int32_t reader_index,
                                       uint8_t *cw_out, const S_ACCOUNT *acc,
                                       bool require_group, S_CW_CACHE_WAIT *wait);
bool cw_cache_wait(S_CW_CACHE_WAIT *wait, const uint8_t *ecm_md5, uint8_t *cw_out,
                   const S_ACCOUNT *acc, bool require_group);
void cw_cache_complete_reader(const uint8_t *ecm_md5, int32_t reader_index, bool success);
void cw_cache_store_groups(const uint8_t *ecm_md5, const uint8_t *cw,
                           const int32_t *groups, int32_t ngroups);

#endif
