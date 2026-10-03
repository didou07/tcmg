#define MODULE_LOG_PREFIX "account"
#include "account.h"
#include "config/config.h"
#include "account_state.h"
#include "core/utils.h"
#include "crypto/crypto.h"
#include "log/log.h"
#include "stats/account_stats.h"
#include "security/antishare.h"

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

S_ACCOUNT *account_clone_config(const S_ACCOUNT *src)
{
    S_ACCOUNT *dst;
    if (!src) return NULL;

    dst = (S_ACCOUNT *)calloc(1, sizeof(*dst));
    if (!dst) return NULL;

    memcpy(dst->user, src->user, sizeof(dst->user));
    memcpy(dst->pass, src->pass, sizeof(dst->pass));
    dst->caid = src->caid;
    dst->group = src->group;
    memcpy(dst->groups, src->groups, sizeof(dst->groups));
    dst->ngroups = src->ngroups;
    dst->enabled = src->enabled;
    memcpy(dst->caids, src->caids, sizeof(dst->caids));
    dst->ncaids = src->ncaids;
    memcpy(dst->idents, src->idents, sizeof(dst->idents));
    dst->nidents = src->nidents;
    memcpy(dst->ip_whitelist, src->ip_whitelist, sizeof(dst->ip_whitelist));
    dst->nwhitelist = src->nwhitelist;
    memcpy(dst->keys, src->keys, sizeof(dst->keys));
    dst->nkeys = src->nkeys;
    dst->max_connections = src->max_connections;
    dst->expirationdate = src->expirationdate;
    dst->max_idle = src->max_idle;
    memcpy(dst->schedule, src->schedule, sizeof(dst->schedule));
    dst->sched_day_from = src->sched_day_from;
    dst->sched_day_to = src->sched_day_to;
    dst->sched_hhmm_from = src->sched_hhmm_from;
    dst->sched_hhmm_to = src->sched_hhmm_to;
    memcpy(dst->sid_whitelist, src->sid_whitelist, sizeof(dst->sid_whitelist));
    dst->nsid_whitelist = src->nsid_whitelist;
    dst->anti_share = src->anti_share;
    dst->as_max_sids = src->as_max_sids;
    dst->as_max_ecm = src->as_max_ecm;
    dst->as_ecm_window_s = src->as_ecm_window_s;
    dst->as_channel_timeout_s = src->as_channel_timeout_s;
    dst->as_switch_delay_s = src->as_switch_delay_s;

    if (!account_stats_init(&dst->stats)) {
        free(dst);
        return NULL;
    }
    if (pthread_mutex_init(&dst->as_mtx, NULL) != 0) {
        account_stats_destroy(&dst->stats);
        free(dst);
        return NULL;
    }
    atomic_init(&dst->active, 0);
    atomic_init(&dst->refs, 0);
    antishare_copy_runtime(dst, src);
    return dst;
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
