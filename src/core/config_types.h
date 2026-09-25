#ifndef TCMG_CONFIG_TYPES_H_
#define TCMG_CONFIG_TYPES_H_

#include "account_types.h"
#include "reader_types.h"
#include "ban_types.h"

typedef struct {
    int32_t  sock_timeout;
    int32_t  server_keepalive;
    int32_t  server_keepalive_misses;
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
    char     cccam_bindaddr[MAXIPLEN];

    int32_t  cs378x_port;
    char     cs378x_bindaddr[MAXIPLEN];

    int32_t  newcamd_port;
    char     newcamd_bindaddr[MAXIPLEN];
    uint8_t  newcamd_key[14];
    int8_t   newcamd_keepalive;
    int8_t   newcamd_mgclient;

    char      config_file[CFGPATH_LEN];
    char      user_file[CFGPATH_LEN];
    char      server_file[CFGPATH_LEN];
    int8_t    pcsc_enabled;
    int32_t   pcsc_fast_reset;
    int32_t   pcsc_poll_ms;
    char      pcsc_reader[CFGVAL_LEN];
    S_READER  readers[MAX_READERS];
    int32_t   nreaders;
    S_ACCOUNT *accounts;
    int32_t   naccounts;
    pthread_rwlock_t acc_lock;
    S_BAN_ENTRY *ban_table[BAN_BUCKETS];
    pthread_mutex_t ban_lock;
    int8_t    failban_enabled;
    char      failban_allowlist[CFGVAL_LEN];
    int32_t   failban_max_fails;
    int32_t   failban_ban_secs;
} S_CONFIG;

#endif
