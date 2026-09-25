#define MODULE_LOG_PREFIX "webif-service"
#include "service.h"
#include "../../src/account/account.h"
#include "../../src/config/config.h"
#include "../../src/core/config_state.h"
#include "../../src/core/runtime_state.h"
#include "../../src/core/utils.h"
#include "../../src/client/client.h"
#include "../../src/stats/account_stats.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void copy_account(const S_ACCOUNT *a, S_WEBIF_ACCOUNT_VIEW *v)
{
    S_ACCOUNT_STATS_SNAPSHOT s;
    memset(v, 0, sizeof(*v));
    tcmg_strlcpy(v->user, a->user, sizeof(v->user));
    tcmg_strlcpy(v->pass, a->pass, sizeof(v->pass));
    for (int i = 0; i < a->ngroups && i < MAX_GROUPS_PER_ACC; i++) {
        char x[24];
        if (i) tcmg_strlcat(v->groups, ",", sizeof(v->groups));
        snprintf(x, sizeof(x), "%d", a->groups[i]);
        tcmg_strlcat(v->groups, x, sizeof(v->groups));
    }
    if (!v->groups[0]) snprintf(v->groups, sizeof(v->groups), "%d", a->group);
    if (a->caid) snprintf(v->caids, sizeof(v->caids), "%04X", a->caid);
    for (int i = 0; i < a->ncaids && i < MAX_CAIDS_PER_ACC; i++) {
        char x[8];
        if (v->caids[0]) tcmg_strlcat(v->caids, ",", sizeof(v->caids));
        snprintf(x, sizeof(x), "%04X", a->caids[i]);
        tcmg_strlcat(v->caids, x, sizeof(v->caids));
    }
    v->enabled = a->enabled;
    v->max_connections = a->max_connections;
    v->active = atomic_load(&a->active);
    v->anti_share = a->anti_share;
    v->as_max_sids = a->as_max_sids;
    v->as_max_ecm = a->as_max_ecm;
    v->as_ecm_window_s = a->as_ecm_window_s;
    v->as_channel_timeout_s = a->as_channel_timeout_s;
    v->as_switch_delay_s = a->as_switch_delay_s;
    v->expirationdate = a->expirationdate;
    account_stats_snapshot(a, &s);
    v->cw_found = s.cw_found;
    v->cw_not = s.cw_not;
    v->ecm_total = s.ecm_total;
    v->cw_time_total_ms = s.cw_time_total_ms;
    v->cw_time_min_ms = s.cw_time_min_ms;
    v->cw_time_max_ms = s.cw_time_max_ms;
    v->first_login = s.first_login;
    v->last_seen = s.last_seen;
    tcmg_strlcpy(v->last_ip, s.last_ip, sizeof(v->last_ip));
}
static void copy_account_userstats(const S_ACCOUNT *a, S_WEBIF_USER_STATS_VIEW *v)
{
    S_ACCOUNT_STATS_SNAPSHOT s;
    memset(v, 0, sizeof(*v));
    tcmg_strlcpy(v->user, a->user, sizeof(v->user));
    if (a->caid) snprintf(v->caids, sizeof(v->caids), "%04X", a->caid);
    for (int i = 0; i < a->ncaids && i < MAX_CAIDS_PER_ACC; i++) {
        char x[8];
        if (v->caids[0]) tcmg_strlcat(v->caids, ",", sizeof(v->caids));
        snprintf(x, sizeof(x), "%04X", a->caids[i]);
        tcmg_strlcat(v->caids, x, sizeof(v->caids));
    }
    v->enabled = a->enabled;
    account_stats_snapshot(a, &s);
    v->cw_found = s.cw_found;
    v->cw_not = s.cw_not;
    v->cw_time_total_ms = s.cw_time_total_ms;
    v->last_seen = s.last_seen;
    tcmg_strlcpy(v->last_ip, s.last_ip, sizeof(v->last_ip));
}

int webif_account_userstats_snapshot_all(S_WEBIF_USER_STATS_VIEW *out, size_t cap)
{
    int n = 0;
    if (!out || cap == 0) return 0;
    pthread_rwlock_rdlock(&g_cfg.acc_lock);
    for (S_ACCOUNT *a = g_cfg.accounts; a && (size_t)n < cap; a = a->next)
        copy_account_userstats(a, &out[n++]);
    pthread_rwlock_unlock(&g_cfg.acc_lock);
    return n;
}

int webif_account_count(void)
{
    int n;
    pthread_rwlock_rdlock(&g_cfg.acc_lock); n = g_cfg.naccounts; pthread_rwlock_unlock(&g_cfg.acc_lock);
    return n;
}

int webif_account_snapshot_all(S_WEBIF_ACCOUNT_VIEW *out, size_t cap)
{
    int n = 0;
    if (!out || cap == 0) return 0;
    pthread_rwlock_rdlock(&g_cfg.acc_lock);
    for (S_ACCOUNT *a = g_cfg.accounts; a && (size_t)n < cap; a = a->next) copy_account(a, &out[n++]);
    pthread_rwlock_unlock(&g_cfg.acc_lock);
    return n;
}

bool webif_account_get(const char *user, S_WEBIF_ACCOUNT_VIEW *out)
{
    if (!user || !out) return false;
    pthread_rwlock_rdlock(&g_cfg.acc_lock);
    for (S_ACCOUNT *a = g_cfg.accounts; a; a = a->next) {
        if (!strcmp(a->user, user)) { copy_account(a, out); pthread_rwlock_unlock(&g_cfg.acc_lock); return true; }
    }
    pthread_rwlock_unlock(&g_cfg.acc_lock);
    return false;
}

bool webif_account_toggle(const char *user, int *enabled)
{
    if (!user || !enabled) return false;
    pthread_rwlock_wrlock(&g_cfg.acc_lock);
    S_ACCOUNT *a = g_cfg.accounts;
    while (a && strcmp(a->user, user)) a = a->next;
    if (!a) { pthread_rwlock_unlock(&g_cfg.acc_lock); return false; }
    a->enabled = !a->enabled;
    *enabled = a->enabled;
    pthread_rwlock_unlock(&g_cfg.acc_lock);
    bool ok = cfg_save(&g_cfg);
    if (ok && !*enabled) client_kill_by_user(user);
    return ok;
}

bool webif_account_save(const S_WEBIF_ACCOUNT_EDIT *f)
{
    if (!f) return false;
    pthread_rwlock_wrlock(&g_cfg.acc_lock);
    S_ACCOUNT *a = g_cfg.accounts;
    while (a && strcmp(a->user, f->user)) a = a->next;
    if (!a) { pthread_rwlock_unlock(&g_cfg.acc_lock); return false; }
    tcmg_strlcpy(a->pass, f->pass, sizeof(a->pass));
    memset(a->groups, 0, sizeof(a->groups));
    a->ngroups = f->ngroups;
    for (int i = 0; i < f->ngroups; i++) a->groups[i] = f->groupv[i];
    a->group = f->ngroups > 0 ? f->groupv[0] : 1;
    if (f->has_caid) {
        a->caid = f->caidv[0]; a->ncaids = 0; memset(a->caids, 0, sizeof(a->caids));
        for (int i = 1; i < f->ncaidv; i++) a->caids[a->ncaids++] = f->caidv[i];
    }
    if (f->has_max_connections) a->max_connections = f->max_connections;
    if (f->has_enabled) a->enabled = f->enabled;
    if (f->has_anti_share) a->anti_share = f->anti_share;
    a->as_max_sids = f->as_max_sids;
    a->as_max_ecm = f->as_max_ecm;
    a->as_ecm_window_s = f->as_ecm_window_s;
    a->as_channel_timeout_s = f->as_channel_timeout_s;
    a->as_switch_delay_s = f->as_switch_delay_s;
    a->expirationdate = f->expiry;
    int disabled = !a->enabled;
    pthread_rwlock_unlock(&g_cfg.acc_lock);
    bool ok = cfg_save(&g_cfg);
    if (ok && disabled) client_kill_by_user(f->user);
    return ok;
}

bool webif_account_add(const S_WEBIF_ACCOUNT_EDIT *f, int *status_code)
{
    if (status_code) *status_code = 500;
    if (!f) return false;
    pthread_rwlock_wrlock(&g_cfg.acc_lock);
    for (S_ACCOUNT *a = g_cfg.accounts; a; a = a->next) {
        if (!strcmp(a->user, f->user)) {
            pthread_rwlock_unlock(&g_cfg.acc_lock);
            if (status_code) *status_code = 409;
            return false;
        }
    }
    S_ACCOUNT *a = cfg_account_new(&g_cfg);
    if (!a) { pthread_rwlock_unlock(&g_cfg.acc_lock); return false; }
    tcmg_strlcpy(a->user, f->user, sizeof(a->user));
    tcmg_strlcpy(a->pass, f->pass, sizeof(a->pass));
    a->ngroups = f->ngroups; for (int i = 0; i < f->ngroups; i++) a->groups[i] = f->groupv[i]; a->group = f->ngroups > 0 ? f->groupv[0] : 1;
    if (f->has_caid) {
        a->caid = f->caidv[0]; a->ncaids = 0; memset(a->caids, 0, sizeof(a->caids)); for (int i = 1; i < f->ncaidv; i++) a->caids[a->ncaids++] = f->caidv[i];
    }
    if (f->has_max_connections) a->max_connections = f->max_connections;
    a->enabled = f->has_enabled ? f->enabled : 1;
    a->anti_share = f->has_anti_share ? f->anti_share : 0;
    a->as_max_sids = f->as_max_sids; a->as_max_ecm = f->as_max_ecm; a->as_ecm_window_s = f->as_ecm_window_s; a->as_channel_timeout_s = f->as_channel_timeout_s; a->as_switch_delay_s = f->as_switch_delay_s; a->expirationdate = f->expiry;
    pthread_rwlock_unlock(&g_cfg.acc_lock);
    bool ok = cfg_save(&g_cfg);
    if (status_code) *status_code = ok ? 200 : 500;
    return ok;
}

bool webif_account_delete(const char *user)
{
    if (!user || !*user) return false;
    S_ACCOUNT *removed = NULL;
    pthread_rwlock_wrlock(&g_cfg.acc_lock);
    S_ACCOUNT **pp = &g_cfg.accounts;
    while (*pp) {
        if (!strcmp((*pp)->user, user)) { removed = *pp; *pp = removed->next; removed->next = NULL; removed->enabled = 0; g_cfg.naccounts--; break; }
        pp = &(*pp)->next;
    }
    if (removed) {
        pthread_rwlock_unlock(&g_cfg.acc_lock);
        bool ok = cfg_save(&g_cfg);
        if (!ok) return false;
        client_kill_by_user(user);
        account_retire(removed);
        account_reap_retired();
        return true;
    }
    pthread_rwlock_unlock(&g_cfg.acc_lock);
    return false;
}

bool webif_account_reset_stats(const char *user)
{
    if (!user || !*user) return false;
    pthread_rwlock_rdlock(&g_cfg.acc_lock);
    S_ACCOUNT *a = g_cfg.accounts;
    while (a && strcmp(a->user, user)) a = a->next;
    if (!a) { pthread_rwlock_unlock(&g_cfg.acc_lock); return false; }
    account_stats_reset(a);
    pthread_rwlock_unlock(&g_cfg.acc_lock);
    return true;
}

void webif_account_reset_all_stats(void)
{
    pthread_rwlock_rdlock(&g_cfg.acc_lock);
    for (S_ACCOUNT *a = g_cfg.accounts; a; a = a->next) account_stats_reset(a);
    pthread_rwlock_unlock(&g_cfg.acc_lock);
}

