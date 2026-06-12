#define MODULE_LOG_PREFIX "client"
#include "../../globals.h"


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
}

void client_kill_by_tid(uint32_t tid)
{
	pthread_mutex_lock(&g_clients_mtx);
	for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++)
		if (g_clients[i] && g_clients[i]->thread_id == tid)
			{ g_clients[i]->kill_flag = 1; break; }
	pthread_mutex_unlock(&g_clients_mtx);
}

void client_kill_by_user(const char *username)
{
	pthread_mutex_lock(&g_clients_mtx);
	for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++)
		if (g_clients[i] && strcmp(g_clients[i]->user, username) == 0)
			g_clients[i]->kill_flag = 1;
	pthread_mutex_unlock(&g_clients_mtx);
}

void clients_relink_accounts(void)
{
	pthread_mutex_lock(&g_clients_mtx);
	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++)
	{
		S_CLIENT *cl = g_clients[i];
		if (!cl || !cl->user[0]) continue;
		S_ACCOUNT *a;
		for (a = g_cfg.accounts; a; a = a->next)
			if (strcmp(cl->user, a->user) == 0) break;
		cl->account = a;
		if (!a) cl->kill_flag = 1;
	}
	pthread_rwlock_unlock(&g_cfg.acc_lock);
	pthread_mutex_unlock(&g_clients_mtx);
}
