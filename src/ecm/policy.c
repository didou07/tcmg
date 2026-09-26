#define MODULE_LOG_PREFIX "ecm"
#include "policy.h"
#include "account/account.h"
#include "security/antishare.h"
#include <time.h>

T_ECM_ACCESS_STATUS ecm_access(S_CLIENT *client, uint16_t caid, uint16_t sid,
                               bool check_schedule, bool check_caid, bool check_sid)
{
    S_ACCOUNT *account;
    if (!client || !(account = client->auth.account)) return ECM_ACCESS_NO_ACCOUNT;
    if (!account->enabled) return ECM_ACCESS_DISABLED;
    if (account->expirationdate > 0 && time(NULL) > account->expirationdate)
        return ECM_ACCESS_EXPIRED;
    if (check_schedule && !account_in_schedule(account)) return ECM_ACCESS_SCHEDULE;
    if (check_caid && !account_allows_caid(account, caid)) return ECM_ACCESS_CAID_DENIED;
    if (check_sid && !account_allows_sid(account, sid)) return ECM_ACCESS_SID_DENIED;

    client->ecm.antishare_delay_ms = 0;
    client->ecm.antishare_prepared = 0;
    int anti_delay_ms = 0;
    if (antishare_check_request(account, client->identity.thread_id, caid, sid, &anti_delay_ms) != AS_CHECK_OK)
        return ECM_ACCESS_ANTISHARE;
    client->ecm.antishare_delay_ms = anti_delay_ms;
    client->ecm.antishare_prepared = 1;
    return ECM_ACCESS_OK;
}
