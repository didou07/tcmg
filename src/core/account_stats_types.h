#ifndef TCMG_ACCOUNT_STATS_TYPES_H_
#define TCMG_ACCOUNT_STATS_TYPES_H_

#include "compat.h"
#include "constants.h"
#include <pthread.h>
#include <stdint.h>
#include <stdatomic.h>
#include <time.h>

typedef struct {
    uint64_t ecm_total;
    int64_t  cw_found;
    int64_t  cw_not;
    int64_t  cw_time_total_ms;
    time_t   last_seen;
    char     last_ip[MAXIPLEN];
    time_t   first_login;
    int64_t  cw_time_min_ms;
    int64_t  cw_time_max_ms;
} S_ACCOUNT_STATS_SNAPSHOT;

typedef struct {
    pthread_mutex_t lock;
    _Atomic int     global_tracked;
    uint64_t        ecm_total;
    int64_t         cw_found;
    int64_t         cw_not;
    int64_t         cw_time_total_ms;
    _Atomic time_t  last_seen;
    char            last_ip[MAXIPLEN];
    time_t          first_login;
    int64_t         cw_time_min_ms;
    int64_t         cw_time_max_ms;
} S_ACCOUNT_STATS;

#endif
