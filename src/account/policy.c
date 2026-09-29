#define MODULE_LOG_PREFIX "account"
#include "account.h"
#include "../reader/rules.h"
#include <time.h>

int account_collect_caids(const S_ACCOUNT *account, uint16_t *out, int32_t cap)
{
    if (!account || !out || cap <= 0) return 0;

    if (account->caid) {
        int32_t count = 1;
        out[0] = account->caid;
        for (int32_t i = 0; i < account->ncaids && count < cap; i++) {
            bool exists = false;
            for (int32_t j = 0; j < count; j++) {
                if (out[j] == account->caids[i]) {
                    exists = true;
                    break;
                }
            }
            if (!exists && account->caids[i]) out[count++] = account->caids[i];
        }
        return count;
    }

    return reader_collect_account_caids(account, out, cap);
}

uint16_t account_default_caid(const S_ACCOUNT *account)
{
    uint16_t caid = 0;
    if (account && account->caid) return account->caid;
    (void)account_collect_caids(account, &caid, 1);
    return caid;
}

bool account_allows_caid(const S_ACCOUNT *account, uint16_t caid)
{
    uint16_t caids[MAX_CAIDS_PER_ACC + (MAX_READERS * MAX_CAIDS_PER_READER)];
    int32_t n;

    if (!account || !caid) return false;
    n = account_collect_caids(account, caids,
                              (int32_t)(sizeof(caids) / sizeof(caids[0])));
    for (int32_t i = 0; i < n; i++)
        if (caids[i] == caid) return true;
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
