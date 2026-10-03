#ifndef TCMG_CW_CACHE_H_
#define TCMG_CW_CACHE_H_

#include "cache_types.h"
#include "../core/account_types.h"
#include "../core/reader_types.h"
#include <stdbool.h>
#include <stdint.h>

bool cw_cache_lookup(const uint8_t *ecm_md5, uint8_t *cw_out,
                     const S_ACCOUNT *acc, bool require_group);
bool cw_cache_lookup_groups(const uint8_t *ecm_md5, uint8_t *cw_out,
                            const S_ACCOUNT *acc, bool require_group,
                            int32_t *groups_out, int32_t *ngroups_out);

void cw_cache_store_groups(const uint8_t *ecm_md5, const uint8_t *cw,
                           const int32_t *groups, int32_t ngroups);

typedef enum {
    CW_INFLIGHT_HIT = 0,
    CW_INFLIGHT_LEADER = 1,
    CW_INFLIGHT_WAIT = 2
} E_CW_INFLIGHT_BEGIN;

typedef struct {
    int32_t slot;
    uint32_t generation;
} S_CW_INFLIGHT_WAIT;

E_CW_INFLIGHT_BEGIN cw_inflight_begin(const uint8_t *ecm_md5,
                                      uint8_t *cw_out,
                                      const S_ACCOUNT *acc,
                                      bool require_group,
                                      S_CW_INFLIGHT_WAIT *wait,
                                      int32_t *groups_out, int32_t *ngroups_out);
bool cw_inflight_wait(S_CW_INFLIGHT_WAIT *wait,
                      const uint8_t *ecm_md5,
                      uint8_t *cw_out,
                      const S_ACCOUNT *acc,
                      bool require_group,
                      int32_t *groups_out, int32_t *ngroups_out);
void cw_inflight_complete(const uint8_t *ecm_md5, bool success);

#endif
