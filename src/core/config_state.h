#ifndef TCMG_CONFIG_STATE_H_
#define TCMG_CONFIG_STATE_H_

#include "config_types.h"
#include <pthread.h>

extern S_CONFIG g_cfg;
extern pthread_mutex_t g_cfg_transition_mtx;

#endif
