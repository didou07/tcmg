#ifndef TCMG_TYPES_H_
#define TCMG_TYPES_H_

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

    _Atomic int32_t   active;
    uint64_t          ecm_total;
    int64_t           cw_found;
    int64_t           cw_not;
    int64_t           cw_time_total_ms;
    _Atomic time_t    last_seen;
    time_t            first_login;
    int64_t           cw_time_min_ms;
    int64_t           cw_time_max_ms;
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

    int32_t  cccam_port;

    int32_t  newcamd_port;
    char     newcamd_bindaddr[MAXIPLEN];
    uint8_t  newcamd_key[14];
    int8_t   newcamd_keepalive;
    int8_t   newcamd_mgclient;

    char             config_file[CFGPATH_LEN];
    S_ACCOUNT       *accounts;
    int32_t          naccounts;
    pthread_rwlock_t acc_lock;
    S_BAN_ENTRY     *ban_table[BAN_BUCKETS];
    pthread_mutex_t  ban_lock;
} S_CONFIG;

typedef struct {
    int         fd;
    char        ip[MAXIPLEN];
    uint16_t    caid;
    uint16_t    client_id;
    int8_t      is_mgcamd;
    char        proto[12];
    uint32_t    thread_id;
    char        user[CFGKEY_LEN];
    char        client_name[32];
    uint8_t     key1[16];
    uint8_t     key2[16];
    uint8_t     session_key[14];
    uint8_t     recv_buf[NC_MSG_MAX];
    uint8_t     send_buf[NC_MSG_MAX + 64];
    S_ACCOUNT  *account;
    uint16_t    last_caid;
    uint16_t    last_srvid;
    char        last_channel[80];
    time_t      connect_time;
    _Atomic time_t  last_ecm_time;
    _Atomic int8_t  kill_flag;
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

typedef struct {
    uint16_t    mask;
    const char *name;
} S_DBLEVEL_NAME;

#endif
