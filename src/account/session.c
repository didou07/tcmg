#define MODULE_LOG_PREFIX "account"
#include "account.h"
#include "config/config.h"
#include "security/antishare.h"
#include "account_state.h"
#include "../core/client_state.h"
#include <stdatomic.h>

int account_session_open(S_CLIENT *client, S_ACCOUNT *account)
{
    if (!client || !account) return -1;

    account_state_write_lock();
    pthread_mutex_lock(&client->auth_mtx);

    if (client->auth.account && (client->auth.account != account || client->auth.counted)) {
        pthread_mutex_unlock(&client->auth_mtx);
        account_state_write_unlock();
        return -1;
    }
    if (account->max_connections > 0 &&
        atomic_load(&account->active) >= account->max_connections) {
        pthread_mutex_unlock(&client->auth_mtx);
        account_state_write_unlock();
        return -1;
    }

    atomic_fetch_add(&account->active, 1);
    client->auth.account = account;
    client->auth.counted = 1;

    pthread_mutex_unlock(&client->auth_mtx);
    account_state_write_unlock();
    return 0;
}

void account_session_close(S_CLIENT *client)
{
    S_ACCOUNT *account;
    int8_t counted;
    if (!client) return;

    pthread_mutex_lock(&client->auth_mtx);
    account = client->auth.account;
    counted = client->auth.counted;
    client->auth.account = NULL;
    client->auth.counted = 0;
    pthread_mutex_unlock(&client->auth_mtx);

    if (!account) return;
    antishare_release_client(account, client->identity.thread_id);
    if (counted) atomic_fetch_sub(&account->active, 1);
    account_release(account);
}

void account_session_rebind(S_CLIENT *client, S_ACCOUNT *account)
{
    S_ACCOUNT *old;
    int8_t old_counted;
    if (!client || !account) return;

    pthread_mutex_lock(&client->auth_mtx);
    old = client->auth.account;
    old_counted = client->auth.counted;
    if (old == account) {
        pthread_mutex_unlock(&client->auth_mtx);
        return;
    }

    account_retain(account);
    atomic_fetch_add(&account->active, 1);
    client->auth.account = account;
    client->auth.counted = 1;
    pthread_mutex_unlock(&client->auth_mtx);

    if (old) {
        antishare_release_client(old, client->identity.thread_id);
        if (old_counted) atomic_fetch_sub(&old->active, 1);
        account_release(old);
    }
}

S_ACCOUNT *account_session_acquire(S_CLIENT *client)
{
    S_ACCOUNT *account = NULL;
    if (!client) return NULL;
    pthread_mutex_lock(&client->auth_mtx);
    account = client->auth.account;
    if (account) account_retain(account);
    pthread_mutex_unlock(&client->auth_mtx);
    return account;
}
