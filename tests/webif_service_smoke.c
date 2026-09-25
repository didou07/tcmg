#include "globals.h"
#include "webif/service/service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int write_text(const char *path, const char *s)
{
    FILE *f = fopen(path, "wb");
    size_t n;
    if (!f) return 0;
    n = strlen(s);
    if (fwrite(s, 1, n, f) != n) { fclose(f); return 0; }
    return fclose(f) == 0;
}

static int init_global_config(const char *dir)
{
    char path[512];
    const char *global =
        "[global]\n"
        "socket_timeout = 30\nserver_keepalive = 20\nserver_keepalive_misses = 3\n"
        "ecm_log = 0\nlogfile =\nusrfile =\n"
        "[webif]\nenabled = 1\nport = 18080\nrefresh = 7\nuser = admin\npassword = secret\nbindaddr = 127.0.0.1\n"
        "[pcsc]\nenabled = 0\nreader =\nfast_reset = 0\npoll_ms = 250\n"
        "[newcamd]\nport = 19050\nbindaddr =\nkey = 0102030405060708091011121314\nkeepalive = 1\nmode = auto\n"
        "[cccam]\nport = 19060\nbindaddr =\n"
        "[cs378x]\nport = 19070\nbindaddr =\n"
        "[failban]\nenabled = 0\nallowlist =\nmax_fails = 5\nban_secs = 300\n";
    const char *users =
        "[account]\nuser = admin\npwd = pass\nenabled = 1\n"
        "group = 1, 7\ncaid = 0B00,0604\nmax_connections = 3\nexpiration = 0\n";
    const char *readers =
        "[reader]\nlabel = emu\nprotocol = emu\nenabled = 1\n"
        "group = 1\ncaid = 0B00\ninactivitytimeout = 30\n"
        "ecmkey = 0B00=00112233445566778899AABBCCDDEEFF00112233445566778899AABBCCDDEEFF\n";

    snprintf(path, sizeof(path), "%s/tcmg.conf", dir);
    if (!write_text(path, global)) return 0;
    snprintf(path, sizeof(path), "%s/tcmg.users", dir);
    if (!write_text(path, users)) return 0;
    snprintf(path, sizeof(path), "%s/tcmg.readers", dir);
    if (!write_text(path, readers)) return 0;

    memset(&g_cfg, 0, sizeof(g_cfg));
    if (pthread_rwlock_init(&g_cfg.acc_lock, NULL) != 0) return 0;
    if (pthread_mutex_init(&g_cfg.ban_lock, NULL) != 0) {
        pthread_rwlock_destroy(&g_cfg.acc_lock);
        return 0;
    }
    strncpy(g_cfgdir, dir, sizeof(g_cfgdir) - 1);
    g_cfgdir[sizeof(g_cfgdir) - 1] = 0;
    g_start_time = time(NULL) - 123;
    snprintf(path, sizeof(path), "%s/tcmg.conf", dir);
    return cfg_load(path, &g_cfg) ? 1 : 0;
}

static void cleanup_global_config(void)
{
    cfg_accounts_free(&g_cfg);
    pthread_rwlock_destroy(&g_cfg.acc_lock);
    pthread_mutex_destroy(&g_cfg.ban_lock);
}

int main(void)
{
    char dir[] = "/tmp/tcmg-webif-service-XXXXXX";
    S_WEBIF_CONFIG_VIEW cfg;
    S_WEBIF_ACCOUNT_VIEW accounts[2];
    S_WEBIF_READER_VIEW readers[2];
    S_WEBIF_ACCOUNT_VIEW one_account;
    S_WEBIF_READER_VIEW one_reader;
    S_WEBIF_READER_EDIT edit;
    S_WEBIF_SERVER_STATS stats;
    int n, index = -1;
    int enabled = -1;

    if (!mkdtemp(dir)) return 2;
    if (!init_global_config(dir)) { rmdir(dir); return 3; }

    if (!webif_config_snapshot(&cfg) || cfg.webif_port != 18080 ||
        cfg.webif_refresh != 7 || strcmp(cfg.webif_bindaddr, "127.0.0.1") != 0 ||
        cfg.newcamd_port != 19050 || cfg.newcamd_key[0] != 0x01) return 4;

    n = webif_account_snapshot_all(accounts, 2);
    if (n != 1 || strcmp(accounts[0].user, "admin") != 0 ||
        accounts[0].enabled != 1 || strcmp(accounts[0].groups, "1,7") != 0 ||
        strcmp(accounts[0].caids, "0B00,0604") != 0) return 5;
    if (!webif_account_get("admin", &one_account) || one_account.max_connections != 3) return 6;
    if (webif_account_get("missing", &one_account)) return 7;

    n = webif_reader_snapshot_all(readers, 2);
    if (n != 1 || strcmp(readers[0].label, "emu") != 0 ||
        strcmp(readers[0].protocol, "emu") != 0 || strcmp(readers[0].groups, "1") != 0 ||
        strcmp(readers[0].caids, "0B00") != 0 || readers[0].ecm_whitelist != 255) return 8;
    if (!webif_reader_get(0, &one_reader) || strcmp(one_reader.ecmkeys,
        "0B00=00112233445566778899AABBCCDDEEFF00112233445566778899AABBCCDDEEFF") != 0) return 9;
    if (webif_reader_get(9, &one_reader)) return 10;
    if (!webif_reader_first_free(&index) || index < 1) return 11;

    if (!webif_auth_enabled() || !webif_credentials_valid("admin", "secret") ||
        webif_credentials_valid("admin", "wrong") ||
        !webif_auth_basic_valid("Basic YWRtaW46c2VjcmV0")) return 12;
    if (webif_port() != 18080 || webif_max_refresh() != 7) return 13;
    {
        char bind[64];
        webif_bindaddr(bind, sizeof(bind));
        if (strcmp(bind, "127.0.0.1") != 0) return 14;
    }

    stats = webif_server_stats();
    if (stats.naccounts != 1 || stats.uptime_s < 120 || stats.uptime_s > 130) return 15;
    if (webif_pcsc_enabled() != 0) return 16;

    memset(&edit, 0, sizeof(edit));
    edit.index = index;
    snprintf(edit.label, sizeof(edit.label), "bad-group");
    snprintf(edit.protocol, sizeof(edit.protocol), "emu");
    snprintf(edit.enabled, sizeof(edit.enabled), "1");
    snprintf(edit.ecmwhitelist, sizeof(edit.ecmwhitelist), "37");
    snprintf(edit.group, sizeof(edit.group), "1x");
    snprintf(edit.caid, sizeof(edit.caid), "0B00");
    if (webif_reader_save(&edit)) return 17;

    memset(&edit, 0, sizeof(edit));
    edit.index = index;
    snprintf(edit.label, sizeof(edit.label), "max-len");
    snprintf(edit.protocol, sizeof(edit.protocol), "emu");
    snprintf(edit.enabled, sizeof(edit.enabled), "1");
    snprintf(edit.ecmwhitelist, sizeof(edit.ecmwhitelist), "FF");
    snprintf(edit.group, sizeof(edit.group), "1");
    snprintf(edit.caid, sizeof(edit.caid), "0B00");
    snprintf(edit.ecmkeys, sizeof(edit.ecmkeys), "0B00=00112233445566778899AABBCCDDEEFF00112233445566778899AABBCCDDEEFF");
    if (!webif_reader_save(&edit)) return 18;
    if (!webif_reader_get(index, &one_reader) || one_reader.ecm_whitelist != 255) return 19;

    cleanup_global_config();
    unlink("/tmp/tcmg-webif-service-does-not-exist");
    {
        char path[512];
        snprintf(path, sizeof(path), "%s/tcmg.conf", dir); remove(path);
        snprintf(path, sizeof(path), "%s/tcmg.users", dir); remove(path);
        snprintf(path, sizeof(path), "%s/tcmg.readers", dir); remove(path);
    }
    rmdir(dir);
    printf("WEBIF_SERVICE_SMOKE: PASS\n");
    (void)enabled;
    return 0;
}
