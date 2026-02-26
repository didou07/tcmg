#ifndef TCMG_GLOBALS_H_
#define TCMG_GLOBALS_H_

/* Platform detection */
#if defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__) || defined(__MINGW64__)
#  define TCMG_OS_WINDOWS  1
#elif defined(__APPLE__) && defined(__MACH__)
#  define TCMG_OS_MACOS    1
#  define TCMG_OS_POSIX    1
#else
#  define TCMG_OS_LINUX    1
#  define TCMG_OS_POSIX    1
#endif

/* Windows: winsock + compat */
#ifdef TCMG_OS_WINDOWS

#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0601   /* Windows 7+ */
#  endif

#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  include <io.h>
#  include <pthread.h>

#  define close(fd)       closesocket(fd)
#  define MSG_NOSIGNAL    0
#  define ssize_t         int
#  define socklen_t       int

static inline struct tm *localtime_r(const time_t *t, struct tm *tm_s)
{
	localtime_s(tm_s, t);
	return tm_s;
}

static inline struct tm *gmtime_r(const time_t *t, struct tm *tm_s)
{
	gmtime_s(tm_s, t);
	return tm_s;
}

#if !defined(__MINGW32__) && !defined(__MINGW64__)
static inline size_t strnlen(const char *s, size_t n)
{
	const char *p = memchr(s, 0, n);
	return p ? (size_t)(p - s) : n;
}
#endif

static inline unsigned int sleep(unsigned int s)
{
	Sleep(s * 1000u);
	return 0;
}

static inline int tcmg_winsock_init(void)
{
	WSADATA wd;
	return WSAStartup(MAKEWORD(2, 2), &wd);
}
static inline void tcmg_winsock_cleanup(void)
{
	WSACleanup();
}

#else /* POSIX */

#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif

#  include <pthread.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <poll.h>
#  include <sys/socket.h>
#  include <sys/time.h>
#  include <sys/select.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <arpa/inet.h>
#  include <netdb.h>

#  define tcmg_winsock_init()    do { } while(0)
#  define tcmg_winsock_cleanup() do { } while(0)

#endif /* platform */

/* Winsock pointer-type compat for setsockopt/send/recv */
#if defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__) || defined(__MINGW64__)
#  define SO_CAST(p)    ((const char *)(p))
#  define RECV_CAST(p)  ((char *)(p))
#else
#  define SO_CAST(p)    (p)
#  define RECV_CAST(p)  (p)
#endif

/* Common system headers */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <signal.h>

#ifdef TCMG_OS_MACOS
#  include <sys/types.h>
#  ifndef MSG_NOSIGNAL
#    define MSG_NOSIGNAL  0
#  endif
#endif

/* Build-time constants */
#define TCMG_VERSION         "4.0"
#define TCMG_BANNER          "tcmg v" TCMG_VERSION
#define CW_LEN               16
#define NC_MSG_MAX           400
#define NC_HDR_LEN           8
#define LOG_RING_MAX         2000
#define MAX_CONNS            256
#define BAN_MAX_FAILS        5
#define BAN_SECS             300
#define MAXIPLEN             INET_ADDRSTRLEN
#define MAX_ECMKEYS_PER_ACC  8
#define MAX_IP_WHITELIST     16
#define MAX_CAIDS_PER_ACC    8
#define CFGKEY_LEN           64
#define CFGVAL_LEN           256
#define CFGPATH_LEN          512
#define MAX_SID_WHITELIST    64
#define CW_CACHE_SIZE        512   /* must be power of two */
#define CW_CACHE_TTL_S       30

#ifndef TCMG_BUILD_TIME
#  define TCMG_BUILD_TIME    __DATE__ " " __TIME__
#endif

#define TCMG_CFG_FILE        "config.cfg"
#define TCMG_SRVID_FILE      "tcmg.srvid2"

/* Newcamd protocol message IDs */
#define MSG_CLIENT_LOGIN     0xe0
#define MSG_CLIENT_LOGIN_ACK 0xe1
#define MSG_CLIENT_LOGIN_NAK 0xe2
#define MSG_CARD_DATA_REQ    0xe3
#define MSG_CARD_DATA        0xe4
#define MSG_KEEPALIVE        0x8d
#define MSG_ADDCARD          0xD3
#define MSG_GET_VERSION      0xD6
#define MSG_ECM_0            0x80
#define MSG_ECM_1            0x81

/* Failban reason masks */
#define BAN_UNKNOWN          1
#define BAN_BADPASS          2
#define BAN_DISABLED         4

/* Emulator result codes */
enum e_emu_result
{
	EMU_OK             = 0,
	EMU_NOT_SUPPORTED  = 1,
	EMU_KEY_NOT_FOUND  = 2,
	EMU_CHECKSUM_ERROR = 3,
};

/* ECM CW cache entry: MD5(ECM) → CW */
typedef struct s_cw_cache_entry
{
	uint8_t ecm_md5[16];
	uint8_t cw[CW_LEN];
	time_t  ts;
	int8_t  valid;
} S_CW_CACHE_ENTRY;

/* Per-account ECM key pair */
typedef struct s_ecmkey
{
	uint16_t caid;
	uint8_t  key0[16];
	uint8_t  key1[16];
} S_ECMKEY;

/* Account (one per [account] block in config) */
typedef struct s_account
{
	char     user[CFGKEY_LEN];
	char     pass[CFGKEY_LEN];
	uint16_t caid;
	int32_t  group;
	int8_t   enabled;
	int8_t   use_fake_cw;

	uint16_t caids[MAX_CAIDS_PER_ACC];
	int32_t  ncaids;

	char     ip_whitelist[MAX_IP_WHITELIST][MAXIPLEN];
	int32_t  nwhitelist;

	S_ECMKEY keys[MAX_ECMKEYS_PER_ACC];
	int32_t  nkeys;

	int32_t  max_connections;    /* 0 = unlimited */
	time_t   expirationdate;     /* 0 = never */
	int32_t  max_idle;           /* 0 = unlimited (seconds) */

	/* Timeframe filter: "MON-FRI 08:00-22:00", empty = always */
	char     schedule[64];
	int8_t   sched_day_from;     /* -1 = not set; 0=Mon..6=Sun */
	int8_t   sched_day_to;
	int16_t  sched_hhmm_from;
	int16_t  sched_hhmm_to;

	uint16_t sid_whitelist[MAX_SID_WHITELIST];
	int32_t  nsid_whitelist;     /* 0 = all SIDs allowed */

	/* Runtime counters (atomic) */
	volatile int32_t  active;
	volatile uint64_t ecm_total;
	volatile int64_t  cw_found;
	volatile int64_t  cw_not;
	volatile int64_t  cw_time_total_ms;
	volatile time_t   last_seen;
	volatile time_t   first_login;

	struct s_account *next;
} S_ACCOUNT;

/* Fail-ban entry */
typedef struct s_ban_entry
{
	char    ip[MAXIPLEN];
	int32_t fails;
	time_t  until;
	struct s_ban_entry *next;
} S_BAN_ENTRY;

/* Global server config */
typedef struct s_config
{
	/* [server] */
	int32_t  port;
	int32_t  sock_timeout;
	int8_t   ecm_log;
	uint8_t  des_key[14];
	char     logfile[CFGPATH_LEN];

	/* [webif] */
	int8_t   webif_enabled;
	int32_t  webif_port;
	int32_t  webif_refresh;
	char     webif_user[CFGKEY_LEN];
	char     webif_pass[CFGKEY_LEN];
	char     webif_bindaddr[MAXIPLEN];

	/* runtime — not persisted */
	char           config_file[CFGPATH_LEN];
	S_ACCOUNT     *accounts;
	int32_t        naccounts;
	pthread_rwlock_t acc_lock;

	S_BAN_ENTRY   *bans;
	pthread_mutex_t ban_lock;
} S_CONFIG;

/* Per-connection client state */
typedef struct s_client
{
	int          fd;
	char         ip[MAXIPLEN];
	uint16_t     caid;
	uint16_t     client_id;
	int8_t       is_mgcamd;
	uint32_t     thread_id;
	char         user[CFGKEY_LEN];
	char         client_name[32];
	uint8_t      key1[16];
	uint8_t      key2[16];
	uint8_t      session_key[14];
	uint8_t      recv_buf[NC_MSG_MAX];
	uint8_t      send_buf[NC_MSG_MAX + 64];
	S_ACCOUNT   *account;

	uint16_t     last_caid;
	uint16_t     last_srvid;
	char         last_channel[80];
	time_t       connect_time;
	volatile time_t last_ecm_time;
	volatile int8_t kill_flag;
} S_CLIENT;

/* ECM context passed to emulator */
typedef struct s_ecm_ctx
{
	int          fd;
	char         user[CFGKEY_LEN];
	char         ip[MAXIPLEN];
	uint16_t     caid;
	uint32_t     thread_id;
	S_ACCOUNT   *account;
} S_ECM_CTX;

/* Per-thread connect arg */
typedef struct s_conn_args
{
	int  fd;
	char ip[MAXIPLEN];
} S_CONN_ARGS;

/* Global singletons (defined in tcmg.c) */
extern S_CONFIG         g_cfg;
extern volatile int32_t g_running;
extern volatile int32_t g_reload_cfg;
extern volatile int32_t g_active_conns;
extern time_t           g_start_time;
extern char             g_cfgdir[CFGPATH_LEN];

/* ECM CW cache */
extern S_CW_CACHE_ENTRY  g_cw_cache[CW_CACHE_SIZE];
extern pthread_mutex_t   g_cw_cache_mtx;

/* Sub-module headers */
#include "tcmg-log.h"
#include "tcmg-net.h"
#include "tcmg-crypto.h"
#include "tcmg-conf.h"
#include "tcmg-emu.h"
#include "tcmg-ban.h"
#include "tcmg-webif.h"
#include "tcmg-srvid2.h"

/* Client registry (defined in tcmg.c) */
#define MAX_ACTIVE_CLIENTS 256
extern S_CLIENT        *g_clients[MAX_ACTIVE_CLIENTS];
extern pthread_mutex_t  g_clients_mtx;
void client_kill_by_tid(uint32_t tid);

/* Uptime formatter */
static inline void format_uptime(time_t seconds, char *out, size_t sz)
{
	int d = (int)(seconds / 86400);
	int h = (int)((seconds % 86400) / 3600);
	int m = (int)((seconds % 3600) / 60);
	int s = (int)(seconds % 60);
	if (d > 0) snprintf(out, sz, "%dd %02dh %02dm %02ds", d, h, m, s);
	else        snprintf(out, sz, "%02dh %02dm %02ds", h, m, s);
}

/* Time → "YYYY-MM-DD HH:MM" */
static inline void format_time(time_t t, char *out, size_t sz)
{
	if (!t) { strncpy(out, "never", sz); return; }
	struct tm tm; localtime_r(&t, &tm);
	strftime(out, sz, "%Y-%m-%d %H:%M", &tm);
}

#endif /* TCMG_GLOBALS_H_ */
