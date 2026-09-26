#include "core/config_state.h"
#include "core/account_types.h"
#include <pthread.h>
#include "account/account_state.h"
#include "core/config_state.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    S_ACCOUNT a;
    S_ACCOUNT b;

    memset(&g_cfg, 0, sizeof(g_cfg));
    if (pthread_rwlock_init(&g_cfg.acc_lock, NULL) != 0) return 2;

    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    snprintf(a.user, sizeof(a.user), "alpha");
    snprintf(b.user, sizeof(b.user), "beta");
    a.next = &b;
    g_cfg.accounts = &a;
    g_cfg.naccounts = 2;

    account_state_read_lock();
    if (account_state_list_locked() != &a || account_state_count_locked() != 2) {
        account_state_read_unlock();
        pthread_rwlock_destroy(&g_cfg.acc_lock);
        return 3;
    }
    account_state_read_unlock();

    account_state_write_lock();
    g_cfg.naccounts = 1;
    account_state_write_unlock();

    account_state_read_lock();
    if (account_state_count_locked() != 1) {
        account_state_read_unlock();
        pthread_rwlock_destroy(&g_cfg.acc_lock);
        return 4;
    }
    account_state_read_unlock();

    g_cfg.accounts = NULL;
    pthread_rwlock_destroy(&g_cfg.acc_lock);
    printf("ACCOUNT_STATE: PASS\n");
    return 0;
}
