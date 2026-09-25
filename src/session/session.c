#define MODULE_LOG_PREFIX "session"
#include "session.h"
#include "account/account.h"
#include "client/client.h"
#include "core/runtime_state.h"
#include "core/utils.h"
#include "log/log.h"
#include <unistd.h>

void session_init(S_CLIENT *client, const T_SESSION_INPUT *input, const char *proto)
{
    if (!client || !input) return;
    client_init(client, input->fd, input->ip);
    client->identity.thread_id = (uint32_t)(uintptr_t)pthread_self();
    if (proto) tcmg_strlcpy(client->protocol.name, proto, sizeof(client->protocol.name));
    log_set_type(LOG_TYPE_CLIENT);
    client_register(client);
}

bool session_idle_expired(const S_CLIENT *client, time_t now)
{
    time_t last;
    if (!client || !client->auth.account || client->auth.account->max_idle <= 0) return false;
    last = client->session.last_activity ? client->session.last_activity : client->ecm.last_ecm_time;
    return now - last >= client->auth.account->max_idle;
}

void session_cleanup(S_CLIENT *client)
{
    int fd;
    if (!client) return;
    fd = client->session.fd;
    account_session_close(client);
    client_unregister(client);
    if (fd >= 0) close(fd);
    atomic_fetch_sub(&g_active_conns, 1);
}
