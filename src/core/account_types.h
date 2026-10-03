#ifndef TCMG_ACCOUNT_TYPES_H_
#define TCMG_ACCOUNT_TYPES_H_

#include "compat.h"
#include "constants.h"
#include "account_stats_types.h"
#include "ident_types.h"

#ifndef TCMG_ECMKEY_DEFINED
#define TCMG_ECMKEY_DEFINED
typedef struct {
    uint16_t caid;
    uint8_t  key0[16];
    uint8_t  key1[16];
} S_ECMKEY;
#endif

typedef struct s_account {
    char     user[CFGKEY_LEN];
    char     pass[CFGKEY_LEN];
    uint16_t caid;
    int32_t  group;
    int32_t  groups[MAX_GROUPS_PER_ACC];
    int32_t  ngroups;
    bool     enabled;

    uint16_t caids[MAX_CAIDS_PER_ACC];
    int32_t  ncaids;
    S_IDENT_FILTER idents[MAX_IDENT_FILTERS];
    int32_t  nidents;

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

#define AS_MAX_CHANNELS 32
#define AS_MAX_RECENT_CHANNELS 16
    bool     anti_share;
    int32_t  as_max_sids;
    int32_t  as_max_ecm;
    int32_t  as_ecm_window_s;
    int32_t  as_channel_timeout_s;
    int32_t  as_switch_delay_s;
    struct {
        uint32_t client_tid;
        uint16_t caid;
        uint16_t sid;
        int64_t  last_success_ms;
        int64_t  pending_since_ms;
        uint8_t  pending;
    } as_channels[AS_MAX_CHANNELS];
    int32_t  as_ecm_points;
    int32_t  as_ecm_active;
    int64_t  as_ecm_window_start_ms;
    uint8_t  as_last_cw[CW_LEN];
    uint16_t as_last_cw_caid;
    uint16_t as_last_cw_sid;
    int64_t  as_last_cw_ms;
    int32_t  as_channel_count;
    struct {
        uint16_t caid;
        uint16_t sid;
        uint16_t hits;
        int64_t  first_seen_ms;
        int64_t  last_seen_ms;
    } as_recent_channels[AS_MAX_RECENT_CHANNELS];
    int32_t  as_recent_count;
    int32_t  as_suspicion_score;
    int64_t  as_suspicion_last_ms;
    pthread_mutex_t as_mtx;

    _Atomic int32_t active;
    _Atomic uint32_t refs;
    S_ACCOUNT_STATS    stats;

    struct s_account *next;
} S_ACCOUNT;

#endif
