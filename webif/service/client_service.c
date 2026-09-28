#define MODULE_LOG_PREFIX "webif"
#include "service.h"
#include "../../src/client/client.h"
#include "../../src/core/runtime_state.h"
#include "../../src/core/client_state.h"
#include "../../src/core/client_types.h"
#include "../../src/core/config_state.h"
#include "../../src/core/utils.h"
#include <pthread.h>
#include <string.h>
int webif_active_connection_count(void)
{
    return atomic_load_explicit(&g_active_conns, memory_order_relaxed);
}

static int snapshot_client(S_WEBIF_CLIENT_VIEW *out, int n, const S_CLIENT *cl)
{
    tcmg_strlcpy(out[n].user, cl->identity.user, sizeof(out[n].user));
    if (!out[n].user[0]) tcmg_strlcpy(out[n].user, cl->auth.account->user, sizeof(out[n].user));
    tcmg_strlcpy(out[n].ip, cl->identity.ip, sizeof(out[n].ip));
    tcmg_strlcpy(out[n].proto, cl->protocol.name[0] ? cl->protocol.name : "unknown", sizeof(out[n].proto));
    tcmg_strlcpy(out[n].channel, cl->ecm.last_channel, sizeof(out[n].channel));
    out[n].caid = cl->ecm.last_caid;
    out[n].sid = cl->ecm.last_srvid;
    out[n].thread_id = cl->identity.thread_id;
    out[n].connect_time = cl->session.connect_time;
    out[n].last_activity = cl->session.last_activity ? cl->session.last_activity :
        (cl->ecm.last_ecm_time ? cl->ecm.last_ecm_time : cl->session.connect_time);
    return n + 1;
}

int webif_client_snapshot_all(S_WEBIF_CLIENT_VIEW *out, size_t cap)
{
    int n = 0;
    if (!out || !cap) return 0;
    pthread_mutex_lock(&g_clients_mtx);
    for (int i = 0; i < MAX_ACTIVE_CLIENTS && (size_t)n < cap; i++) {
        S_CLIENT *cl = g_clients[i];
        if (!cl || !cl->auth.account) continue;
        n = snapshot_client(out, n, cl);
    }
    pthread_mutex_unlock(&g_clients_mtx);
    return n;
}

int webif_client_snapshot_alloc(S_WEBIF_CLIENT_VIEW **out)
{
    if (!out) return -1;
    *out = NULL;
    int count = 0;
    pthread_mutex_lock(&g_clients_mtx);
    for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++) {
        S_CLIENT *cl = g_clients[i];
        if (cl && cl->auth.account) count++;
    }
    if (count > 0) {
        S_WEBIF_CLIENT_VIEW *snap = malloc((size_t)count * sizeof(*snap));
        if (!snap) {
            pthread_mutex_unlock(&g_clients_mtx);
            return -1;
        }
        int n = 0;
        for (int i = 0; i < MAX_ACTIVE_CLIENTS && n < count; i++) {
            S_CLIENT *cl = g_clients[i];
            if (!cl || !cl->auth.account) continue;
            n = snapshot_client(snap, n, cl);
        }
        *out = snap;
        pthread_mutex_unlock(&g_clients_mtx);
        return n;
    }
    pthread_mutex_unlock(&g_clients_mtx);
    return 0;
}

void webif_client_kill_by_tid(uint32_t tid) { client_kill_by_tid(tid); }
void webif_client_kill_by_user(const char *user) { client_kill_by_user(user); }
