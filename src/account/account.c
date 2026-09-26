#define MODULE_LOG_PREFIX "account"
#include "account.h"
#include "config/config.h"
#include "account_state.h"
#include "core/utils.h"
#include "crypto/crypto.h"
#include "log/log.h"
#include "stats/account_stats.h"

static S_ACCOUNT *s_retired_accounts;
static pthread_mutex_t s_retired_mtx = PTHREAD_MUTEX_INITIALIZER;

static void free_account_list(S_ACCOUNT *head)
{
    while (head) {
        S_ACCOUNT *next = head->next;
        account_stats_global_remove(head);
        account_stats_destroy(&head->stats);
        pthread_mutex_destroy(&head->as_mtx);
        secure_zero(head, sizeof(*head));
        free(head);
        head = next;
    }
}

S_ACCOUNT *account_acquire(const char *user)
{
    S_ACCOUNT *found = NULL;
    if (!user || !*user) return NULL;

    account_state_read_lock();
    for (S_ACCOUNT *account = account_state_list_locked(); account; account = account->next) {
        if (strcmp(account->user, user) == 0) {
            atomic_fetch_add(&account->refs, 1);
            found = account;
            break;
        }
    }
    account_state_read_unlock();
    return found;
}

void account_retain(S_ACCOUNT *account)
{
    if (account) atomic_fetch_add(&account->refs, 1);
}

void account_retire(S_ACCOUNT *account)
{
    if (!account) return;

    account_stats_global_remove(account);
    pthread_mutex_lock(&s_retired_mtx);
    account->next = s_retired_accounts;
    s_retired_accounts = account;
    pthread_mutex_unlock(&s_retired_mtx);
    account_reap_retired();
}

void account_reap_retired(void)
{
    pthread_mutex_lock(&s_retired_mtx);

    S_ACCOUNT **pp = &s_retired_accounts;
    while (*pp) {
        S_ACCOUNT *account = *pp;
        if (atomic_load(&account->active) == 0 && atomic_load(&account->refs) == 0) {
            *pp = account->next;
            account->next = NULL;
            account_stats_destroy(&account->stats);
            pthread_mutex_destroy(&account->as_mtx);
            secure_zero(account, sizeof(*account));
            free(account);
            continue;
        }
        pp = &account->next;
    }

    pthread_mutex_unlock(&s_retired_mtx);
}

void account_retired_free(void)
{
    pthread_mutex_lock(&s_retired_mtx);
    free_account_list(s_retired_accounts);
    s_retired_accounts = NULL;
    pthread_mutex_unlock(&s_retired_mtx);
}

void account_release(S_ACCOUNT *account)
{
    unsigned prev;
    if (!account) return;

    prev = atomic_fetch_sub(&account->refs, 1);
    if (prev == 0) {
        atomic_store(&account->refs, 0);
        tcmg_log("reference underflow for user='%s'", account->user);
        return;
    }
    if (prev == 1) account_reap_retired();
}
