#ifndef TCMG_TYPES_H_
#define TCMG_TYPES_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#define CW_LEN               16
#define NC_MSG_MAX           400
#define NC_HDR_LEN           8
#define LOG_RING_MAX         2000
#define MAX_CONNS            256
#define BAN_MAX_FAILS        5
#define BAN_SECS             300
#define MAXIPLEN             16   /* INET_ADDRSTRLEN */
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

    /* live counters — updated atomically */
    volatile int32_t  active;
    volatile uint64_t ecm_total;
    volatile int64_t  cw_found;
    volatile int64_t  cw_not;
    volatile int64_t  cw_time_total_ms;
    volatile time_t   last_seen;
    volatile time_t   first_login;
    volatile int64_t  cw_time_min_ms;
    volatile int64_t  cw_time_max_ms;

    struct s_account *next;
} S_ACCOUNT;

typedef struct s_ban_entry {
    char    ip[MAXIPLEN];
    int32_t fails;
    time_t  until;
    struct s_ban_entry *next;
} S_BAN_ENTRY;

typedef struct {
    int32_t  port;
    int32_t  sock_timeout;
    int8_t   ecm_log;
    uint8_t  des_key[14];
    char     logfile[CFGPATH_LEN];
    char     usrfile[CFGPATH_LEN];

    int8_t   webif_enabled;
    int32_t  webif_port;
    int32_t  webif_refresh;
    char     webif_user[CFGKEY_LEN];
    char     webif_pass[CFGKEY_LEN];
    char     webif_bindaddr[MAXIPLEN];

    char             config_file[CFGPATH_LEN];
    S_ACCOUNT       *accounts;
    int32_t          naccounts;
    pthread_rwlock_t acc_lock;
    S_BAN_ENTRY     *bans;
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

#endif /* TCMG_TYPES_H_ */
