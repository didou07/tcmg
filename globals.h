#ifndef GLOBALS_H_
#define GLOBALS_H_

#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__)
#  ifndef _POSIX_THREAD_SAFE_FUNCTIONS
#    define _POSIX_THREAD_SAFE_FUNCTIONS 200112L
#  endif
#else
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#endif

#if defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__) || defined(__MINGW64__)
#  define TCMG_OS_WINDOWS 1
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0601
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  include <io.h>
#  define close(fd)    closesocket(fd)
#  define MSG_NOSIGNAL 0
#  define ssize_t      int
#  define socklen_t    int
#  if !defined(__MINGW32__) && !defined(__MINGW64__)
     static inline struct tm *localtime_r(const time_t *t, struct tm *s)
         { localtime_s(s, t); return s; }
     static inline struct tm *gmtime_r(const time_t *t, struct tm *s)
         { gmtime_s(s, t); return s; }
     static inline size_t strnlen(const char *s, size_t n)
         { const char *p = (const char *)memchr(s, 0, n);
           return p ? (size_t)(p - s) : n; }
     static inline unsigned int sleep(unsigned int sec)
         { Sleep(sec * 1000u); return 0; }
#  else
#    include <unistd.h>
#  endif
   static inline int  tcmg_winsock_init(void)
       { WSADATA w; return WSAStartup(MAKEWORD(2,2), &w); }
   static inline void tcmg_winsock_cleanup(void) { WSACleanup(); }
#else
#  define TCMG_OS_POSIX 1
#  include <unistd.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <sys/socket.h>
#  include <sys/time.h>
#  include <sys/select.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <poll.h>
#  ifdef __APPLE__
#    ifndef MSG_NOSIGNAL
#      define MSG_NOSIGNAL 0
#    endif
#  endif
#  define tcmg_winsock_init()    ((void)0)
#  define tcmg_winsock_cleanup() ((void)0)
#endif

#include <pthread.h>
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

#ifdef TCMG_OS_WINDOWS
#  define SO_CAST(p)   ((const char *)(p))
#  define RECV_CAST(p) ((char *)(p))
#else
#  define SO_CAST(p)   (p)
#  define RECV_CAST(p) (p)
#endif

#if defined(TCMG_OS_WINDOWS)
#  define tcmg_sleep_ms(ms) Sleep((DWORD)(ms))
#else
static inline void tcmg_sleep_ms(int ms)
{
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}
#endif

#ifndef TCMG_VERSION
#  define TCMG_VERSION     "5.3"
#endif
#define TCMG_BANNER        "tcmg v" TCMG_VERSION
#ifndef TCMG_BUILD_TIME
#  define TCMG_BUILD_TIME  __DATE__ " " __TIME__
#endif
#ifndef CS_CONFDIR
#  ifdef TCMG_OS_WINDOWS
#    define CS_CONFDIR "."
#  else
#    define CS_CONFDIR "/usr/local/etc"
#  endif
#endif
#define TCMG_CFG_FILE      "tcmg.conf"
#define TCMG_SRVID_FILE    "tcmg.srvid2"

#define CW_LEN               16
#define NC_MSG_MAX           1024
#define NC_HDR_LEN           8
#define LOG_RING_MAX         4000
#define MAX_CONNS            256
#define BAN_MAX_FAILS        5
#define BAN_SECS             300
#define MAXIPLEN             16
#define MAX_ECMKEYS_PER_ACC  8
#define MAX_IP_WHITELIST     16
#define MAX_CAIDS_PER_ACC    8
#define CFGKEY_LEN           64
#define CFGVAL_LEN           256
#define CFGPATH_LEN          512
#define MAX_SID_WHITELIST    64
#define CW_CACHE_SIZE        512
#define CW_CACHE_TTL_S       30
#define MAX_ACTIVE_CLIENTS   256
#define BAN_BUCKETS          256

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

typedef enum {
    EMU_OK             = 0,
    EMU_NOT_SUPPORTED  = 1,
    EMU_KEY_NOT_FOUND  = 2,
    EMU_CHECKSUM_ERROR = 3,
} e_emu_result;

typedef struct {
    uint8_t ecm_md5[16];
    uint8_t cw[CW_LEN];
    time_t  ts;
    int8_t  valid;
} S_CW_CACHE_ENTRY;

typedef struct {
    uint16_t caid;
    uint8_t  key0[16];
    uint8_t  key1[16];
} S_ECMKEY;

typedef struct s_account {
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

    int32_t  max_connections;
    time_t   expirationdate;
    int32_t  max_idle;

    char     schedule[64];
    int8_t   sched_day_from;
    int8_t   sched_day_to;
    int16_t  sched_hhmm_from;
    int16_t  sched_hhmm_to;

    uint16_t sid_whitelist[MAX_SID_WHITELIST];
    int32_t  nsid_whitelist;

    volatile int32_t  active;
    volatile uint64_t ecm_total;
    volatile int64_t  cw_found;
    volatile int64_t  cw_not;
    volatile int64_t  cw_time_total_ms;
    volatile time_t   last_seen;
    volatile time_t   first_login;
    volatile int64_t  cw_time_min_ms;
    volatile int64_t  cw_time_max_ms;
    pthread_mutex_t   stat_mtx;

    struct s_account *next;
} S_ACCOUNT;

typedef struct s_ban_entry {
    char    ip[MAXIPLEN];
    int32_t fails;
    time_t  until;
    struct s_ban_entry *next;
} S_BAN_ENTRY;

typedef struct {
    int32_t  sock_timeout;
    int8_t   ecm_log;
    char     logfile[CFGPATH_LEN];
    char     usrfile[CFGPATH_LEN];

    int8_t   webif_enabled;
    int32_t  webif_port;
    int32_t  webif_refresh;
    char     webif_user[CFGKEY_LEN];
    char     webif_pass[CFGKEY_LEN];
    char     webif_bindaddr[MAXIPLEN];

    int32_t          cccam_port;

    int32_t          newcamd_port;
    char             newcamd_bindaddr[MAXIPLEN];
    uint8_t          newcamd_key[14];
    int8_t           newcamd_keepalive;
    int8_t           newcamd_mgclient;

    char             config_file[CFGPATH_LEN];
    S_ACCOUNT       *accounts;
    int32_t          naccounts;
    pthread_rwlock_t acc_lock;
    S_BAN_ENTRY     *ban_table[BAN_BUCKETS];
    pthread_mutex_t  ban_lock;
} S_CONFIG;

typedef struct {
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

typedef struct {
    int       fd;
    char      user[CFGKEY_LEN];
    char      ip[MAXIPLEN];
    uint16_t  caid;
    uint32_t  thread_id;
    S_ACCOUNT *account;
} S_ECM_CTX;

typedef struct {
    int  fd;
    char ip[MAXIPLEN];
} S_CONN_ARGS;

#define CW_CACHE_SHARDS 16

extern S_CONFIG          g_cfg;
extern volatile int32_t  g_running;
extern volatile int32_t  g_reload_cfg;
extern volatile int32_t  g_restart;
extern volatile int32_t  g_active_conns;
extern time_t            g_start_time;
extern char              g_cfgdir[CFGPATH_LEN];
extern S_CW_CACHE_ENTRY  g_cw_cache[CW_CACHE_SIZE];
extern pthread_mutex_t   g_cw_cache_mtx[CW_CACHE_SHARDS];
extern S_CLIENT         *g_clients[MAX_ACTIVE_CLIENTS];
extern pthread_mutex_t   g_clients_mtx;

static inline size_t tcmg_strlcpy(char *dst, const char *src, size_t sz)
{
    size_t n = 0;
    if (sz > 0) {
        for (; n < sz - 1 && src[n]; n++) dst[n] = src[n];
        dst[n] = '\0';
    }
    return n;
}

static inline size_t tcmg_strlcat(char *dst, const char *src, size_t sz)
{
    size_t dlen = strnlen(dst, sz);
    if (dlen >= sz) return sz;
    return dlen + tcmg_strlcpy(dst + dlen, src, sz - dlen);
}

static inline void format_uptime(time_t s, char *out, size_t sz)
{
    int d  = (int)(s / 86400);
    int h  = (int)((s % 86400) / 3600);
    int m  = (int)((s % 3600) / 60);
    int sc = (int)(s % 60);
    if (d > 0) snprintf(out, sz, "%dd %02dh %02dm %02ds", d, h, m, sc);
    else        snprintf(out, sz, "%02dh %02dm %02ds", h, m, sc);
}

static inline void format_time(time_t t, char *out, size_t sz)
{
    if (!t) { tcmg_strlcpy(out, "never", sz); return; }
    struct tm tm_s;
    localtime_r(&t, &tm_s);
    strftime(out, sz, "%Y-%m-%d %H:%M", &tm_s);
}

#ifdef TCMG_OS_WINDOWS
#  define TCMG_PATH_SEP '\\'
#else
#  define TCMG_PATH_SEP '/'
#endif

#include "tcmg-log.h"
#include "tcmg-net.h"
#include "cscrypt/crypto.h"
#include "tcmg-conf.h"
#include "tcmg-emu.h"
#include "tcmg-failban.h"
#include "webif/webif.h"
#include "tcmg-srvid.h"
#include "tcmg-platform.h"
#include "module-cccam.h"
#include "module-newcamd.h"

void client_kill_by_tid(uint32_t tid);

#endif
