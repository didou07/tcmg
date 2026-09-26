/* Test harness: real webif server + synthetic stats and fake clients. */
#define MODULE_LOG_PREFIX "test"
#include "../../src/core/config_state.h"
#include "../../src/core/runtime_state.h"
#include "../../src/core/utils.h"
#include "../../src/client/client.h"
#include "../../src/config/config.h"
#include "../../src/log/log.h"
#include "../../webif/server.h"
#include <signal.h>
#include "../../src/stats/account_stats.h"
#include "../../src/crypto/crypto.h"
#include "../../src/platform/platform.h"

static void set_stats(S_ACCOUNT *a, long ok, long nok, long avg, long mn, long mx,
                      int active, long first_ago, long last_ago)
{
    pthread_mutex_lock(&a->stats.lock);
    a->stats.cw_found = ok; a->stats.cw_not = nok;
    a->stats.cw_time_total_ms = avg * ok; a->stats.cw_time_min_ms = mn; a->stats.cw_time_max_ms = mx;
    a->stats.ecm_total = ok + nok;
    time_t now = time(NULL);
    a->stats.first_login = first_ago ? now - first_ago : 0;
    atomic_store(&a->stats.last_seen, last_ago ? now - last_ago : 0);
    pthread_mutex_unlock(&a->stats.lock);
    atomic_store(&a->active, active);
}

static void add_client(S_ACCOUNT *a, const char *ip, const char *proto, int idle_s, uint32_t tid)
{
    static int slot = 0;
    S_CLIENT *cl = calloc(1, sizeof(*cl));
    cl->auth.account = a;
    snprintf(cl->identity.ip, sizeof(cl->identity.ip), "%s", ip);
    snprintf(cl->protocol.name, sizeof(cl->protocol.name), "%s", proto);
    snprintf(cl->identity.user, sizeof(cl->identity.user), "%s", a->user);
    cl->identity.thread_id = tid;
    cl->session.connect_time = time(NULL) - 3600;
    cl->ecm.last_ecm_time = time(NULL) - idle_s;
    g_clients[slot++] = cl;
}

int main(int argc, char **argv)
{
    g_start_time = time(NULL) - 93784;
    const char *dir = argc > 1 ? argv[1] : "/tmp/tcu";
    snprintf(g_cfgdir, sizeof(g_cfgdir), "%s", dir);
    secure_zero(&g_cfg, sizeof(g_cfg));
    pthread_rwlock_init(&g_cfg.acc_lock, NULL);
    pthread_mutex_init(&g_cfg.ban_lock, NULL);
    char cfgpath[CFGPATH_LEN];
    tcmg_build_path(cfgpath, sizeof(cfgpath), g_cfgdir, TCMG_CFG_FILE);
    if (!cfg_load(cfgpath, &g_cfg)) { fprintf(stderr, "cfg_load failed\n"); return 1; }
    log_init();

    for (S_ACCOUNT *a = g_cfg.accounts; a; a = a->next) {
        const char *u = a->user;
        if      (!strcmp(u,"tvcas"))          { set_stats(a, 48211, 312, 84, 22, 610, 1, 86400*30, 2);   add_client(a,"41.104.22.7","newcamd",3,101); }
        else if (!strcmp(u,"test"))           { set_stats(a, 120, 0, 51, 20, 133, 0, 86400*2, 5000); }
        else if (!strcmp(u,"ahmed_home"))     { set_stats(a, 9120, 88, 97, 30, 900, 2, 86400*12, 1);  add_client(a,"105.99.3.41","cccam",1,102); add_client(a,"192.168.1.20","cccam",12,103); }
        else if (!strcmp(u,"salon_pc"))       { set_stats(a, 640, 310, 143, 40, 2210, 1, 86400*5, 20); add_client(a,"197.200.8.9","newcamd",20,104); }
        else if (!strcmp(u,"old_client"))     { set_stats(a, 2000, 40, 90, 25, 400, 0, 86400*400, 86400*220); }
        else if (!strcmp(u,"suspended_user")) { set_stats(a, 410, 11, 120, 30, 800, 0, 86400*60, 86400*9); }
        else if (!strcmp(u,"bad_signal"))     { set_stats(a, 210, 300, 320, 60, 4100, 1, 86400*3, 40); add_client(a,"154.121.5.60","newcamd",40,105); }
        else if (!strcmp(u,"family"))         { set_stats(a, 15300, 51, 70, 18, 400, 0, 86400*90, 3600*7); }
        else if (!strncmp(u,"a_very_long",11)){ set_stats(a, 33, 2, 110, 40, 300, 0, 86400, 3600*30); }
        else if (!strcmp(u,"cafe_terminal"))  { set_stats(a, 7780, 700, 130, 28, 990, 2, 86400*20, 2); add_client(a,"41.200.10.3","cccam",2,106); add_client(a,"41.200.10.4","cccam",9,107); }
        else if (!strcmp(u,"guest02"))        { set_stats(a, 5, 0, 60, 60, 60, 0, 3600, 3000); }
        else if (!strcmp(u,"reseller_a"))     { set_stats(a, 210000, 1900, 66, 12, 700, 0, 86400*200, 600); }
    }
    webif_start();
    fprintf(stderr, "harness up\n");
    while (1) sleep(1);
}
