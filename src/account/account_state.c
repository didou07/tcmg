#define MODULE_LOG_PREFIX "account"
#include "account_state.h"
#include "../core/config_state.h"
#include <pthread.h>

void account_state_read_lock(void)
{
    pthread_rwlock_rdlock(&g_cfg.acc_lock);
}

void account_state_read_unlock(void)
{
    pthread_rwlock_unlock(&g_cfg.acc_lock);
}

void account_state_write_lock(void)
{
    pthread_rwlock_wrlock(&g_cfg.acc_lock);
}

void account_state_write_unlock(void)
{
    pthread_rwlock_unlock(&g_cfg.acc_lock);
}

S_ACCOUNT *account_state_list_locked(void)
{
    return g_cfg.accounts;
}

int32_t account_state_count_locked(void)
{
    return g_cfg.naccounts;
}
