#define MODULE_LOG_PREFIX "account-session"
#include "account.h"
#include "config/config.h"
#include "security/antishare.h"
#include "account_state.h"
#include <stdatomic.h>

int account_session_open(S_CLIENT *client, S_ACCOUNT *account)
{
    if (!client || !account) return -1;
    if (client->auth.account && (client->auth.account != account || client->auth.counted)) return -1;

    account_state_write_lock();
    if (account->max_connections > 0 &&
        atomic_load(&account->active) >= account->max_connections) {
        account_state_write_unlock();
        return -1;
    }
    atomic_fetch_add(&account->active, 1);
    account_state_write_unlock();

    client->auth.account = account;
    client->auth.counted = 1;
    return 0;
}

void account_session_close(S_CLIENT *client)
{
    S_ACCOUNT *account;
    if (!client) return;
    account = client->auth.account;
    if (!account) return;
    antishare_release_client(account, client->identity.thread_id);

    if (client->auth.counted) {
        atomic_fetch_sub(&account->active, 1);
        client->auth.counted = 0;
    }
    client->auth.account = NULL;
    account_release(account);
}

void account_session_rebind(S_CLIENT *client, S_ACCOUNT *account)
{
    S_ACCOUNT *old;
    int8_t old_counted;
    if (!client || !account || client->auth.account == account) return;

    old = client->auth.account;
    old_counted = client->auth.counted;
    if (old) antishare_release_client(old, client->identity.thread_id);
    atomic_fetch_add(&account->refs, 1);
    atomic_fetch_add(&account->active, 1);
    client->auth.account = account;
    client->auth.counted = 1;

    if (old) {
        if (old_counted) atomic_fetch_sub(&old->active, 1);
        account_release(old);
    }
}
