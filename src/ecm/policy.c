#define MODULE_LOG_PREFIX "ecm"
#include "policy.h"
#include "account/account.h"
#include "security/antishare.h"
#include <time.h>

T_ECM_ACCESS_STATUS ecm_access(S_CLIENT *client, uint16_t caid, uint16_t sid,
                               bool check_schedule, bool check_caid, bool check_sid)
{
    S_ACCOUNT *account;
    T_ECM_ACCESS_STATUS status = ECM_ACCESS_OK;
    int anti_delay_ms = 0;

    if (!client) return ECM_ACCESS_NO_ACCOUNT;
    account = account_session_acquire(client);
    if (!account) return ECM_ACCESS_NO_ACCOUNT;

    if (!account->enabled) status = ECM_ACCESS_DISABLED;
    else if (account->expirationdate > 0 && time(NULL) > account->expirationdate) status = ECM_ACCESS_EXPIRED;
    else if (check_schedule && !account_in_schedule(account)) status = ECM_ACCESS_SCHEDULE;
    else if (check_caid && !account_allows_caid(account, caid)) status = ECM_ACCESS_CAID_DENIED;
    else if (check_sid && !account_allows_sid(account, sid)) status = ECM_ACCESS_SID_DENIED;
    else if (antishare_check_request(account, client->identity.thread_id, caid, sid, &anti_delay_ms) != AS_CHECK_OK) status = ECM_ACCESS_ANTISHARE;

    if (status == ECM_ACCESS_OK) {
        pthread_mutex_lock(&client->state_mtx);
        client->ecm.antishare_delay_ms = anti_delay_ms;
        client->ecm.antishare_prepared = 1;
        pthread_mutex_unlock(&client->state_mtx);
    } else {
        pthread_mutex_lock(&client->state_mtx);
        client->ecm.antishare_delay_ms = 0;
        client->ecm.antishare_prepared = 0;
        pthread_mutex_unlock(&client->state_mtx);
    }
    account_release(account);
    return status;
}
