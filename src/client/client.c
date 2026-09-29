#define MODULE_LOG_PREFIX "client"
#include "client.h"
#include "../account/account.h"
#include "../core/client_state.h"
#include "../account/account_state.h"
#include "../core/utils.h"
#include "../log/log.h"
#include "../crypto/crypto.h"
#include <time.h>

void client_init(S_CLIENT *cl, int fd, const char *ip)
{
    if (!cl) return;
    memset(cl, 0, sizeof(*cl));
    cl->session.fd = fd;
    cl->session.connect_time = time(NULL);
    cl->session.last_activity = cl->session.connect_time;
    cl->ecm.last_ecm_time = cl->session.connect_time;
    (void)pthread_mutex_init(&cl->auth_mtx, NULL);
    (void)pthread_mutex_init(&cl->state_mtx, NULL);
    if (ip) tcmg_strlcpy(cl->identity.ip, ip, sizeof(cl->identity.ip));
}

void client_register(S_CLIENT *cl)
{
	pthread_mutex_lock(&g_clients_mtx);
	for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++)
		if (!g_clients[i]) { g_clients[i] = cl; break; }
	pthread_mutex_unlock(&g_clients_mtx);
}

void client_unregister(S_CLIENT *cl)
{
	pthread_mutex_lock(&g_clients_mtx);
	for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++)
		if (g_clients[i] == cl) { g_clients[i] = NULL; break; }
	pthread_mutex_unlock(&g_clients_mtx);
	if (cl) {
        secure_zero(&cl->protocol.wire, sizeof(cl->protocol.wire));
	}
}

void client_kill_by_tid(uint32_t tid)
{
    pthread_mutex_lock(&g_clients_mtx);
    for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++) {
        S_CLIENT *cl = g_clients[i];
        if (!cl) continue;
        pthread_mutex_lock(&cl->state_mtx);
        if (cl->identity.thread_id == tid) {
            atomic_store_explicit(&cl->session.kill_flag, 1, memory_order_relaxed);
            pthread_mutex_unlock(&cl->state_mtx);
            break;
        }
        pthread_mutex_unlock(&cl->state_mtx);
    }
    pthread_mutex_unlock(&g_clients_mtx);
}

void client_kill_by_user(const char *username)
{
    if (!username) return;
    pthread_mutex_lock(&g_clients_mtx);
    for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++) {
        S_CLIENT *cl = g_clients[i];
        if (!cl) continue;
        pthread_mutex_lock(&cl->state_mtx);
        if (strcmp(cl->identity.user, username) == 0)
            atomic_store_explicit(&cl->session.kill_flag, 1, memory_order_relaxed);
        pthread_mutex_unlock(&cl->state_mtx);
    }
    pthread_mutex_unlock(&g_clients_mtx);
}

void clients_relink_accounts(void)
{
	pthread_mutex_lock(&g_clients_mtx);
	account_state_read_lock();
	for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++)
	{
		S_CLIENT *cl = g_clients[i];
        if (!cl) continue;
        pthread_mutex_lock(&cl->state_mtx);
        int has_user = cl->identity.user[0] != '\0';
        char user[CFGKEY_LEN];
        if (has_user) tcmg_strlcpy(user, cl->identity.user, sizeof(user));
        pthread_mutex_unlock(&cl->state_mtx);
        if (!has_user) continue;
        S_ACCOUNT *a;
        for (a = account_state_list_locked(); a; a = a->next)
            if (strcmp(user, a->user) == 0) break;
        if (a) {
            account_session_rebind(cl, a);
        } else {
            atomic_store_explicit(&cl->session.kill_flag, 1, memory_order_relaxed);
        }
	}
	account_state_read_unlock();
	pthread_mutex_unlock(&g_clients_mtx);
}
