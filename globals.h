#ifndef GLOBALS_H_
#define GLOBALS_H_

#include "src/core/compat.h"
#include "src/core/constants.h"
#include "src/core/types.h"
#include "src/core/utils.h"

#include "src/log/log.h"
#include "src/crypto/crypto.h"
#include "src/config/config.h"
#include "src/security/failban.h"
#include "src/srvid/srvid.h"
#include "src/net/net.h"
#include "src/cache/cw_cache.h"
#include "src/platform/platform.h"
#include "src/proto/cccam.h"
#include "src/proto/newcamd.h"
#include "src/emu/emu.h"
#include "src/client/client.h"
#include "webif/server.h"

extern S_CONFIG          g_cfg;
extern _Atomic int32_t   g_running;
extern _Atomic int32_t   g_reload_cfg;
extern _Atomic int32_t   g_restart;
extern _Atomic int32_t   g_active_conns;
extern time_t            g_start_time;
extern char              g_cfgdir[CFGPATH_LEN];
extern S_CW_CACHE_ENTRY  g_cw_cache[CW_CACHE_SIZE];
extern pthread_mutex_t   g_cw_cache_mtx[CW_CACHE_SHARDS];
extern S_CLIENT         *g_clients[MAX_ACTIVE_CLIENTS];
extern pthread_mutex_t   g_clients_mtx;
extern _Atomic uint16_t  g_dblevel;

void client_kill_by_tid(uint32_t tid);

#endif
