#define MODULE_LOG_PREFIX "account-policy"
#include "account.h"
#include <time.h>

bool account_allows_caid(const S_ACCOUNT *account, uint16_t caid)
{
    if (!account) return false;
    if (account->caid && account->caid == caid) return true;
    for (int32_t i = 0; i < account->ncaids; i++)
        if (account->caids[i] == caid) return true;
    return false;
}

bool account_allows_sid(const S_ACCOUNT *account, uint16_t sid)
{
    if (!account || account->nsid_whitelist <= 0) return true;
    for (int32_t i = 0; i < account->nsid_whitelist; i++)
        if (account->sid_whitelist[i] == sid) return true;
    return false;
}

bool account_in_schedule(const S_ACCOUNT *account)
{
    if (!account || account->sched_day_from < 0) return true;

    time_t now = time(NULL);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    int wday = (tm_buf.tm_wday == 0) ? 6 : (tm_buf.tm_wday - 1);
    int hhmm = tm_buf.tm_hour * 100 + tm_buf.tm_min;
    bool day_ok;

    if (account->sched_day_from <= account->sched_day_to)
        day_ok = wday >= account->sched_day_from && wday <= account->sched_day_to;
    else
        day_ok = wday >= account->sched_day_from || wday <= account->sched_day_to;
    if (!day_ok) return false;

    if (account->sched_hhmm_from <= account->sched_hhmm_to)
        return hhmm >= account->sched_hhmm_from && hhmm < account->sched_hhmm_to;
    return hhmm >= account->sched_hhmm_from || hhmm < account->sched_hhmm_to;
}
