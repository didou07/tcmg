#ifndef TCMG_RUNTIME_STATE_H_
#define TCMG_RUNTIME_STATE_H_

#include "compat.h"
#include "constants.h"

extern _Atomic int32_t g_running;
extern _Atomic int32_t g_reload_cfg;
extern _Atomic int32_t g_restart;
extern _Atomic int32_t g_active_conns;
extern time_t g_start_time;
extern char g_cfgdir[CFGPATH_LEN];

#endif
