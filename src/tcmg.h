#ifndef TCMG_H_
#define TCMG_H_

#include "tcmg_compat.h"
#include "tcmg_types.h"
#include "tcmg_globals.h"
#include "tcmg_helpers.h"

#define TCMG_VERSION     "5.0"
#define TCMG_BANNER      "tcmg v" TCMG_VERSION
#ifndef TCMG_BUILD_TIME
#  define TCMG_BUILD_TIME __DATE__ " " __TIME__
#endif
#ifndef CS_CONFDIR
#  ifdef TCMG_OS_WINDOWS
#    define CS_CONFDIR "."
#  else
#    define CS_CONFDIR "/usr/local/etc"
#  endif
#endif
#define TCMG_CFG_FILE   "tcmg.conf"
#define TCMG_SRVID_FILE "tcmg.srvid2"

#include "core/log.h"
#include "net/net.h"
#include "crypto/crypto.h"
#include "core/conf.h"
#include "core/emu.h"
#include "core/ban.h"
#include "webif/webif.h"
#include "core/srvid.h"
#include "platform/platform.h"

void client_kill_by_tid(uint32_t tid);

#endif /* TCMG_H_ */
