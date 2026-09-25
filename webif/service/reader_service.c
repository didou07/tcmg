#define MODULE_LOG_PREFIX "webif-service"
#include "service.h"
#include "../../src/config/config.h"
#include "../../src/core/config_state.h"
#include "../../src/core/runtime_state.h"
#include "../../src/core/constants.h"
#include "../../src/core/utils.h"
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void copy_reader(const S_READER *r, int idx, S_WEBIF_READER_VIEW *v)
{
    memset(v, 0, sizeof(*v));
    v->index = idx;
    v->enabled = r->enabled;
    v->inactivitytimeout = r->inactivitytimeout;
    v->ecm_whitelist = r->ecm_whitelist;
    v->do_ecm = r->do_ecm;
    v->fast_reset = r->fast_reset;
    v->poll_ms = r->poll_ms;
    tcmg_strlcpy(v->label, r->label, sizeof(v->label));
    tcmg_strlcpy(v->protocol, r->protocol, sizeof(v->protocol));
    tcmg_strlcpy(v->device, r->device, sizeof(v->device));
    tcmg_strlcpy(v->user, r->user, sizeof(v->user));
    tcmg_strlcpy(v->password, r->password, sizeof(v->password));
    for (int i = 0; i < 14; i++) snprintf(v->key + i * 2, 3, "%02X", r->newcamd_key[i]);
    v->key[28] = 0;
    for (int i = 0; i < r->ngroups && i < MAX_GROUPS_PER_READER; i++) {
        char x[24];
        if (i) tcmg_strlcat(v->groups, ",", sizeof(v->groups));
        snprintf(x, sizeof(x), "%d", r->groups[i]);
        tcmg_strlcat(v->groups, x, sizeof(v->groups));
    }
    for (int i = 0; i < r->ncaids && i < MAX_CAIDS_PER_READER; i++) {
        char x[8];
        if (i) tcmg_strlcat(v->caids, ",", sizeof(v->caids));
        snprintf(x, sizeof(x), "%04X", r->caids[i]);
        tcmg_strlcat(v->caids, x, sizeof(v->caids));
    }
    for (int i = 0; i < r->nsid_whitelist && i < MAX_SID_WHITELIST; i++) {
        char x[8];
        if (i) tcmg_strlcat(v->sid_whitelist, ",", sizeof(v->sid_whitelist));
        snprintf(x, sizeof(x), "%04X", r->sid_whitelist[i]);
        tcmg_strlcat(v->sid_whitelist, x, sizeof(v->sid_whitelist));
    }
    for (int i = 0; i < r->nkeys; i++) {
        char hex[65], one[80];
        for (int j = 0; j < 16; j++) snprintf(hex + j * 2, 3, "%02X", r->keys[i].key0[j]);
        for (int j = 0; j < 16; j++) snprintf(hex + 32 + j * 2, 3, "%02X", r->keys[i].key1[j]);
        hex[64] = 0;
        snprintf(one, sizeof(one), "%04X=%s%s", r->keys[i].caid, hex, i + 1 < r->nkeys ? "\n" : "");
        tcmg_strlcat(v->ecmkeys, one, sizeof(v->ecmkeys));
    }
}
bool webif_reader_first_free(int *index)
{
    if (!index) return false;
    bool ok = false;
    pthread_rwlock_rdlock(&g_cfg.acc_lock);
    for (int i = 0; i < MAX_READERS; i++) {
        if (!g_cfg.readers[i].in_use) { *index = i; ok = true; break; }
    }
    pthread_rwlock_unlock(&g_cfg.acc_lock);
    return ok;
}

int webif_reader_count(void)
{
    int n;
    pthread_rwlock_rdlock(&g_cfg.acc_lock); n = g_cfg.nreaders; pthread_rwlock_unlock(&g_cfg.acc_lock);
    return n;
}

int webif_reader_snapshot_all(S_WEBIF_READER_VIEW *out, size_t cap)
{
    int n = 0;
    if (!out || !cap) return 0;
    pthread_rwlock_rdlock(&g_cfg.acc_lock);
    for (int i = 0; i < MAX_READERS && (size_t)n < cap; i++) if (g_cfg.readers[i].in_use) copy_reader(&g_cfg.readers[i], i, &out[n++]);
    pthread_rwlock_unlock(&g_cfg.acc_lock);
    return n;
}

bool webif_reader_get(int index, S_WEBIF_READER_VIEW *out)
{
    if (!out || index < 0 || index >= MAX_READERS) return false;
    pthread_rwlock_rdlock(&g_cfg.acc_lock);
    if (!g_cfg.readers[index].in_use) { pthread_rwlock_unlock(&g_cfg.acc_lock); return false; }
    copy_reader(&g_cfg.readers[index], index, out);
    pthread_rwlock_unlock(&g_cfg.acc_lock);
    return true;
}

static int parse_simple_i32(const char *s, long lo, long hi, int *out)
{
    if (!s || !*s || !out) return -1;
    char *e = NULL; long v = strtol(s, &e, 10);
    if (e == s || *e || v < lo || v > hi) return -1;
    *out = (int)v; return 0;
}

bool webif_reader_save(const S_WEBIF_READER_EDIT *e)
{
    if (!e || e->index < 0 || e->index >= MAX_READERS) return false;
    S_READER value;
    memset(&value, 0, sizeof(value));
    value.in_use = 1;
    value.enabled = 1;
    value.do_ecm = 1;
    value.inactivitytimeout = 30;
    value.ecm_whitelist = 0x37;
    value.poll_ms = 250;
    snprintf(value.label, sizeof(value.label), "reader%d", e->index);
    tcmg_strlcpy(value.protocol, "emu", sizeof(value.protocol));

    tcmg_strlcpy(value.label, e->label, sizeof(value.label));
    tcmg_strlcpy(value.protocol, e->protocol, sizeof(value.protocol));
    tcmg_strlcpy(value.device, e->device, sizeof(value.device));
    tcmg_strlcpy(value.user, e->user, sizeof(value.user));
    tcmg_strlcpy(value.password, e->password, sizeof(value.password));
    int iv;
    if (parse_simple_i32(e->enabled, 0, 1, &iv) < 0) return false;
    value.enabled = (int8_t)iv;
    {
        char *end = NULL; unsigned long x = strtoul(e->ecmwhitelist, &end, 16);
        if (end == e->ecmwhitelist || *end || x > 0xFF) return false;
        value.ecm_whitelist = (int32_t)x;
    }
    if (strcasecmp(e->protocol, "emu") == 0) {
        value.device[0] = 0; value.user[0] = 0; value.password[0] = 0;
        value.inactivitytimeout = 30; value.do_ecm = 1; value.fast_reset = 0; value.poll_ms = 250;
    } else if (strcasecmp(e->protocol, "pcsc") == 0 ||
               strcasecmp(e->protocol, "internal") == 0) {
        value.user[0] = 0; value.password[0] = 0; value.inactivitytimeout = 30;
        if (parse_simple_i32(e->do_ecm, 0, 1, &iv) < 0) return false;
        value.do_ecm = (int8_t)iv;
        if (parse_simple_i32(e->fast_reset, 0, 86400, &value.fast_reset) < 0) return false;
        if (parse_simple_i32(e->poll_ms, 50, 10000, &value.poll_ms) < 0) return false;
    } else {
        if (!e->device[0]) return false;
        if (parse_simple_i32(e->inactivitytimeout, 1, 600, &value.inactivitytimeout) < 0) return false;
    }
        if (strlen(e->group) >= sizeof(e->group) || strlen(e->caid) >= sizeof(e->caid) ||
        strlen(e->sid_whitelist) >= sizeof(e->sid_whitelist) || strlen(e->ecmkeys) >= sizeof(e->ecmkeys)) return false;
    char groups[sizeof(e->group)]; tcmg_strlcpy(groups, e->group, sizeof(groups));
    char *save = NULL, *tok = strtok_r(groups, ",", &save);
    while (tok) {
        char *q = tok; while (*q && isspace((unsigned char)*q)) q++;
        char *end = NULL;
        long x = strtol(q, &end, 10);
        while (end && *end && isspace((unsigned char)*end)) end++;
        if (end == q || (end && *end) || x < 1 || x > 65535 || value.ngroups >= MAX_GROUPS_PER_READER) return false;
        for (int i = 0; i < value.ngroups; i++) if (value.groups[i] == (int32_t)x) return false;
        value.groups[value.ngroups++] = (int32_t)x; tok = strtok_r(NULL, ",", &save);
    }
    if (value.ngroups == 0) return false;

    char caids[sizeof(e->caid)]; tcmg_strlcpy(caids, e->caid, sizeof(caids)); save = NULL; tok = strtok_r(caids, ", ;", &save);
    while (tok) {
        char *end = NULL; unsigned long x = strtoul(tok, &end, 16);
        if (end == tok || *end || x > 0xFFFF || value.ncaids >= MAX_CAIDS_PER_READER) return false;
        for (int i = 0; i < value.ncaids; i++) if (value.caids[i] == (uint16_t)x) return false;
        value.caids[value.ncaids++] = (uint16_t)x; tok = strtok_r(NULL, ", ;", &save);
    }
    char sid[sizeof(e->sid_whitelist)]; tcmg_strlcpy(sid, e->sid_whitelist, sizeof(sid)); save = NULL; tok = strtok_r(sid, ", ;", &save);
    while (tok) {
        char *end = NULL; unsigned long x = strtoul(tok, &end, 16);
        if (end == tok || *end || x > 0xFFFF || value.nsid_whitelist >= MAX_SID_WHITELIST) return false;
        for (int i = 0; i < value.nsid_whitelist; i++) if (value.sid_whitelist[i] == (uint16_t)x) return false;
        value.sid_whitelist[value.nsid_whitelist++] = (uint16_t)x; tok = strtok_r(NULL, ", ;", &save);
    }
    if (strlen(e->key) >= sizeof(value.newcamd_key) * 2 + 1) return false;
    char key[sizeof(e->key)]; tcmg_strlcpy(key, e->key, sizeof(key));
    for (int i = 0; i < 14; i++) { char b[3]={key[i*2],key[i*2+1],0}; if (!isxdigit((unsigned char)b[0]) || !isxdigit((unsigned char)b[1])) { if (strcasecmp(e->protocol,"mgcamd")==0 || strcasecmp(e->protocol,"newcamd")==0) return false; break; } value.newcamd_key[i]=(uint8_t)strtoul(b,NULL,16); }
    char ek[WEBIF_TEXT_8192]; tcmg_strlcpy(ek, e->ecmkeys, sizeof(ek)); save = NULL; tok = strtok_r(ek, "\r\n;", &save);
    while (tok) {
        if (value.nkeys >= MAX_ECMKEYS_PER_ACC) return false;
        char *eq = strchr(tok, '='); if (!eq) return false; *eq++ = 0; char *end = NULL; unsigned long caid = strtoul(tok, &end, 16); if (end==tok||*end||caid>0xFFFF||strlen(eq)!=64) return false;
        for (int i = 0; i < value.nkeys; i++) if (value.keys[i].caid == (uint16_t)caid) return false;
        S_ECMKEY parsed; memset(&parsed, 0, sizeof(parsed)); parsed.caid=(uint16_t)caid;
        for(int j=0;j<64;j++) if(!isxdigit((unsigned char)eq[j])) return false;
        for(int j=0;j<16;j++){char b[3]={eq[j*2],eq[j*2+1],0};parsed.key0[j]=(uint8_t)strtoul(b,NULL,16);b[0]=eq[32+j*2];b[1]=eq[33+j*2];parsed.key1[j]=(uint8_t)strtoul(b,NULL,16);}
        value.keys[value.nkeys++] = parsed;
        tok=strtok_r(NULL,"\r\n;",&save);
    }

    pthread_rwlock_wrlock(&g_cfg.acc_lock);
    g_cfg.readers[e->index] = value;
    g_cfg.nreaders = 0; for (int i = 0; i < MAX_READERS; i++) if (g_cfg.readers[i].in_use) g_cfg.nreaders++;
    pthread_rwlock_unlock(&g_cfg.acc_lock);
    bool ok = cfg_save(&g_cfg);
    if (ok) g_reload_cfg = 1;
    return ok;
}

bool webif_reader_delete(int index)
{
    if (index < 0 || index >= MAX_READERS) return false;
    pthread_rwlock_wrlock(&g_cfg.acc_lock);
    if (!g_cfg.readers[index].in_use) { pthread_rwlock_unlock(&g_cfg.acc_lock); return false; }
    memset(&g_cfg.readers[index], 0, sizeof(g_cfg.readers[index]));
    g_cfg.nreaders = 0; for (int i = 0; i < MAX_READERS; i++) if (g_cfg.readers[i].in_use) g_cfg.nreaders++;
    pthread_rwlock_unlock(&g_cfg.acc_lock);
    bool ok = cfg_save(&g_cfg);
    if (ok) g_reload_cfg = 1;
    return ok;
}
