#include "runtime_state.h"

_Atomic int32_t g_running = 1;
_Atomic int32_t g_reload_cfg = 0;
_Atomic int32_t g_restart = 0;
_Atomic int32_t g_active_conns = 0;
time_t g_start_time = 0;
char g_cfgdir[CFGPATH_LEN] = CS_CONFDIR;
