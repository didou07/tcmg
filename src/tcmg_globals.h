#ifndef TCMG_GLOBALS_H_
#define TCMG_GLOBALS_H_

#include "tcmg_types.h"

extern S_CONFIG          g_cfg;
extern volatile int32_t  g_running;
extern volatile int32_t  g_reload_cfg;
extern volatile int32_t  g_restart;
extern volatile int32_t  g_active_conns;
extern time_t            g_start_time;
extern char              g_cfgdir[CFGPATH_LEN];

extern S_CW_CACHE_ENTRY  g_cw_cache[CW_CACHE_SIZE];
extern pthread_mutex_t   g_cw_cache_mtx;

extern S_CLIENT         *g_clients[MAX_ACTIVE_CLIENTS];
extern pthread_mutex_t   g_clients_mtx;

#endif /* TCMG_GLOBALS_H_ */
