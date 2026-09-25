#include "session/session.h"
#include "account/account.h"
#include "core/config_state.h"
#include "core/runtime_state.h"
#include "core/client_state.h"
#include "client/client.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    S_ACCOUNT account;
    S_CLIENT client;
    T_SESSION_INPUT input = { -1, "192.0.2.20" };
    bool registered = false;

    memset(&account, 0, sizeof(account));
    account.enabled = 1;
    account.max_connections = 2;
    account.max_idle = 60;
    account.sched_day_from = -1;
    atomic_init(&account.active, 0);
    atomic_init(&account.refs, 1);

    assert(pthread_rwlock_init(&g_cfg.acc_lock, NULL) == 0);
    atomic_store(&g_active_conns, 1);

    session_init(&client, &input, "test");
    assert(client.session.fd == -1);
    assert(strcmp(client.identity.ip, "192.0.2.20") == 0);
    assert(strcmp(client.protocol.name, "test") == 0);

    pthread_mutex_lock(&g_clients_mtx);
    for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++)
        if (g_clients[i] == &client) { registered = true; break; }
    pthread_mutex_unlock(&g_clients_mtx);
    assert(registered);

    assert(account_session_open(&client, &account) == 0);
    assert(client.auth.counted == 1);
    assert(atomic_load(&account.active) == 1);
    assert(!session_idle_expired(&client, client.session.last_activity + 30));

    session_cleanup(&client);

    registered = false;
    pthread_mutex_lock(&g_clients_mtx);
    for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++)
        if (g_clients[i] == &client) { registered = true; break; }
    pthread_mutex_unlock(&g_clients_mtx);
    assert(!registered);
    assert(client.auth.account == NULL);
    assert(client.auth.counted == 0);
    assert(atomic_load(&account.active) == 0);
    assert(atomic_load(&account.refs) == 0);
    assert(atomic_load(&g_active_conns) == 0);

    pthread_rwlock_destroy(&g_cfg.acc_lock);
    puts("session_smoke: PASS");
    return 0;
}
