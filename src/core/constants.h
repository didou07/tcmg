#ifndef TCMG_CONSTANTS_H_
#define TCMG_CONSTANTS_H_

#ifndef TCMG_VERSION
#  define TCMG_VERSION    "5.6"
#endif
#define TCMG_BANNER       "tcmg v" TCMG_VERSION
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

#ifdef TCMG_OS_WINDOWS
#  define TCMG_PATH_SEP '\\'
#else
#  define TCMG_PATH_SEP '/'
#endif

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
#define CW_CACHE_SHARDS      16
#define CW_CACHE_TTL_S       30
#define MAX_ACTIVE_CLIENTS   256
#define BAN_BUCKETS          256
#define SRVID_NAME_MAX       80

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

#define D_WIRE      0x0001
#define D_ECM       0x0002
#define D_EMU       0x0004
#define D_NEWCAMD   0x0008
#define D_CCCAM     0x0010
#define D_HTTP      0x0020
#define D_CONN      0x0040
#define D_ALL       0xFFFF

#define MAX_DEBUG_LEVELS 7

typedef enum {
    EMU_OK             = 0,
    EMU_NOT_SUPPORTED  = 1,
    EMU_KEY_NOT_FOUND  = 2,
    EMU_CHECKSUM_ERROR = 3,
} e_emu_result;

#endif
