#ifndef TCMG_CLIENT_STATE_H_
#define TCMG_CLIENT_STATE_H_

#include "client_types.h"
#include <pthread.h>

extern S_CLIENT *g_clients[MAX_ACTIVE_CLIENTS];
extern pthread_mutex_t g_clients_mtx;

#endif
