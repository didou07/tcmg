#include "client_state.h"

S_CLIENT *g_clients[MAX_ACTIVE_CLIENTS];
pthread_mutex_t g_clients_mtx = PTHREAD_MUTEX_INITIALIZER;
