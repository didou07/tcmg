#include "core/config_state.h"
#include "core/runtime_state.h"
#include "config/config.h"
#include "log/log.h"
#include "emu/emu.h"
#include "reader/reader.h"
#include "reader/result.h"
#include "reader/stats.h"
#include "account/account.h"
#include <stdio.h>
#include <string.h>

static void dump16(const char *p, const unsigned char *b) {
    printf("%s", p);
    for (int i=0;i<16;i++) printf("%02X", b[i]);
    putchar('\n');
}

int main(int argc, char **argv) {
    reader_stats_reset(0);
    S_READER_STATS_SNAPSHOT initial_stats;
    reader_stats_snapshot(0, &initial_stats);
    if (initial_stats.cw_ok != 0 || initial_stats.cw_nok != 0 || initial_stats.active) {
        fprintf(stderr,"FAIL initial reader stats\n");
        return 6;
    }
    reader_stats_record(0, true);
    reader_stats_record(0, false);
    S_READER_STATS_SNAPSHOT sample_stats;
    reader_stats_snapshot(0, &sample_stats);
    if (sample_stats.cw_ok != 1 || sample_stats.cw_nok != 1 || !sample_stats.active) {
        fprintf(stderr,"FAIL reader runtime stats\n");
        return 7;
    }
    reader_stats_reset(0);
    bool expect_login_fail = false;
    if (argc == 3 && strcmp(argv[2], "expect-login-fail") == 0) expect_login_fail = true;
    if (argc != 2 && !expect_login_fail) {
        fprintf(stderr,"usage: %s <config-dir/tcmg.conf> [expect-login-fail]\n", argv[0]);
        return 2;
    }
    if (argc == 3 && !expect_login_fail) return 2;
    memset(&g_cfg,0,sizeof(g_cfg));
    pthread_rwlock_init(&g_cfg.acc_lock,NULL);
    pthread_mutex_init(&g_cfg.ban_lock,NULL);
    if (!cfg_load(argv[1], &g_cfg)) { fprintf(stderr,"cfg_load failed\n"); return 3; }
    log_init();
    g_dblevel = UINT16_MAX;
    emu_init();
    S_ACCOUNT *acc = account_acquire("client");
    if (!acc) { fprintf(stderr,"account client not found\n"); return 4; }
    S_ECM_REQUEST req; memset(&req,0,sizeof(req)); req.account=acc; req.user="smoke";
    static const unsigned char ecm[] = {
      0x80,0x00,0x34,0x00,0x32,0x64,0x00,
      0x7C,0x99,0xBB,0xE9,0x60,0x61,0x74,0x1D,0x82,0xC3,0x57,0x47,0xD5,0x26,0x2E,0x0A,0x0E,
      0x76,0x46,0xAB,0x26,0x35,0x50,0x1A,0xF2,0x63,0xD7,0x26,0x1D,0xC1,0x4F,0x0C,0xF2,
      0x63,0xD7,0x26,0x1D,0xC1,0x4F,0x0C,0x16,0x88,0x9C,0x7D,0x01,0x21,0x9A,0x20
    };
    static const unsigned char expected[16] = {
      0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
      0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF
    };
    int overall = 0;
    if (expect_login_fail) {
        unsigned char cw[16]={0};
        S_READER_RESULT result; int32_t rc; req.caid=0x0B00; req.sid=0x0064; req.provid=0; req.ecm=ecm; req.ecm_len=sizeof(ecm); req.cw=cw; rc=reader_dispatch_ecm(&req,&result);
        printf("expected-login-fail rc=%d\n", rc);
        if (rc == 0) {
            fprintf(stderr,"FAIL: login unexpectedly succeeded\n");
            overall = 1;
        }
    }
    else for (int round=1; round<=3; round++) {
        unsigned char cw[16]={0};
        S_READER_RESULT result; int rc; req.caid=0x0B00; req.sid=0x0064; req.provid=0; req.ecm=ecm; req.ecm_len=sizeof(ecm); req.cw=cw; rc=reader_dispatch_ecm(&req,&result);
        printf("round=%d rc=%d ng=%d\n",round,rc,result.ngroups);
        dump16("cw=",cw);
        if (rc != 0 || memcmp(cw, expected, 16) != 0 || result.ngroups != 1 || result.groups[0] != 1) {
            fprintf(stderr,"FAIL round=%d\n",round);
            overall = 1;
        }
    }
    account_release(acc);
    reader_shutdown(); cfg_accounts_free(&g_cfg); pthread_rwlock_destroy(&g_cfg.acc_lock); pthread_mutex_destroy(&g_cfg.ban_lock);
    return overall ? 5 : 0;
}
