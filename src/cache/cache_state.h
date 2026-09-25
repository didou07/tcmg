#ifndef TCMG_CACHE_STATE_H_
#define TCMG_CACHE_STATE_H_

#include "cache_types.h"

extern S_CW_CACHE_ENTRY g_cw_cache[CW_CACHE_SIZE];
extern pthread_mutex_t g_cw_cache_mtx[CW_CACHE_SHARDS];

#endif
