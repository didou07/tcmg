#ifndef TCMG_CW_CACHE_H_
#define TCMG_CW_CACHE_H_

#include "cache_types.h"
#include "../core/account_types.h"

bool cw_cache_lookup(const uint8_t *ecm_md5, uint8_t *cw_out,
                     const S_ACCOUNT *acc, bool require_group);
void cw_cache_store_groups(const uint8_t *ecm_md5, const uint8_t *cw,
                           const int32_t *groups, int32_t ngroups);

#endif
