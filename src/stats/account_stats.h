#ifndef TCMG_ACCOUNT_STATS_H_
#define TCMG_ACCOUNT_STATS_H_

#include "core/account_types.h"
#include "core/account_stats_types.h"
#include <stdbool.h>

bool account_stats_init(S_ACCOUNT_STATS *stats);
void account_stats_destroy(S_ACCOUNT_STATS *stats);
void account_stats_mark_login(S_ACCOUNT *account, const char *ip);
typedef enum {
    ACCOUNT_ECM_NOT_FOUND = 0,
    ACCOUNT_ECM_FOUND = 1,
    ACCOUNT_ECM_REJECTED = 2,
} E_ACCOUNT_ECM_RESULT;

void account_stats_record_ecm(S_ACCOUNT *account, bool found, int64_t elapsed_ms);
void account_stats_record_ecm_result(S_ACCOUNT *account, E_ACCOUNT_ECM_RESULT result, int64_t elapsed_ms);
void account_stats_snapshot(const S_ACCOUNT *account, S_ACCOUNT_STATS_SNAPSHOT *out);
void account_stats_reset(S_ACCOUNT *account);
void account_stats_copy_runtime(S_ACCOUNT *dst, const S_ACCOUNT *src);
void account_stats_global_snapshot(int64_t *cw_found, int64_t *cw_not);
void account_stats_global_remove(S_ACCOUNT *account);
void account_stats_global_adopt(S_ACCOUNT *account);

#endif
