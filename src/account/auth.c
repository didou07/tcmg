#define MODULE_LOG_PREFIX "account"
#include "account.h"
#include "core/constants.h"
#include "core/config_state.h"
#include "core/utils.h"
#include "log/log.h"
#include "stats/account_stats.h"
#include <time.h>

T_ACCOUNT_STATUS account_validate(const S_ACCOUNT *account, const char *ip)
{
    if (!account || !account->enabled) return ACCOUNT_DISABLED;
    if (account->expirationdate > 0 && time(NULL) > account->expirationdate)
        return ACCOUNT_EXPIRED;
    if (account->nwhitelist > 0) {
        bool allowed = false;
        for (int32_t i = 0; i < account->nwhitelist; i++) {
            if (ip && strncmp(account->ip_whitelist[i], ip, MAXIPLEN) == 0) {
                allowed = true;
                break;
            }
        }
        if (!allowed) return ACCOUNT_IP_DENIED;
    }
    return ACCOUNT_OK;
}

void account_mark_login(S_ACCOUNT *account, const char *ip)
{
    account_stats_mark_login(account, ip);
}
