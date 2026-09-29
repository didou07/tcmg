#include "config_state.h"

S_CONFIG g_cfg;
pthread_mutex_t g_cfg_transition_mtx = PTHREAD_MUTEX_INITIALIZER;
