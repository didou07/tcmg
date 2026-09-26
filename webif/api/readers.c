#include "../../src/internal/internal.h"
#define MODULE_LOG_PREFIX "webif"
#include "../../src/serial/serial.h"
#include "../../src/reader/protocol.h"
#include "../service/service.h"
#include "../../src/core/utils.h"
#include "../internal/proto.h"
#include "../internal/form.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char label[READER_LABEL_LEN];
    char protocol[READER_PROTOCOL_LEN];
    char enabled[8];
    char device[CFGVAL_LEN];
    char user[CFGKEY_LEN];
    char password[CFGKEY_LEN];
    char key[32];
    char inactivity[16];
    char caid[128];
    char sid[512];
    char ecmwl[8];
    char groups[256];
    char ecmkeys[WEBIF_TEXT_8192];
    char do_ecm[8];
    char fast_reset[16];
    char poll_ms[16];
} reader_form;

static int rnum(const char *s, long lo, long hi, long *out)
{
    if (!s[0]) return -1;
    char *e = NULL;
    long v = strtol(s, &e, 10);
    if (e == s || *e || v < lo || v > hi) return -1;
    *out = v;
    return 0;
}

static int validate_groups(const char *s)
{
    if (!s[0]) return -1;
    char buf[256]; tcmg_strlcpy(buf, s, sizeof(buf));
    int values[MAX_GROUPS_PER_READER]; int n = 0;
    char *save = NULL, *tok = strtok_r(buf, ",", &save);
    while (tok) {
        webif_trim(tok);
        long v;
        if (!tok[0] || n >= MAX_GROUPS_PER_READER || rnum(tok, 1, 65535, &v) < 0) return -1;
        for (int i = 0; i < n; i++) if (values[i] == (int)v) return -1;
        values[n++] = (int)v;
        tok = strtok_r(NULL, ",", &save);
    }
    return n ? 0 : -1;
}

static int validate_u16_list(const char *s, size_t max_items, size_t bufsz)
{
    if (!s[0]) return 0;
    char *buf = malloc(bufsz);
    if (!buf) return -1;
    tcmg_strlcpy(buf, s, bufsz);
    uint16_t values[64];
    if (max_items > sizeof(values) / sizeof(values[0])) { free(buf); return -1; }
    size_t n = 0;
    char *save = NULL, *tok = strtok_r(buf, ", ;", &save);
    while (tok) {
        webif_trim(tok);
        if (!tok[0] || n >= max_items || strlen(tok) > 4) { free(buf); return -1; }
        char *e = NULL; unsigned long v = strtoul(tok, &e, 16);
        if (e == tok || *e || v > 0xFFFF) { free(buf); return -1; }
        for (size_t i = 0; i < n; i++) if (values[i] == (uint16_t)v) { free(buf); return -1; }
        values[n++] = (uint16_t)v;
        tok = strtok_r(NULL, ", ;", &save);
    }
    free(buf);
    return 0;
}

static int validate_ecmkeys(const char *s)
{
    if (!s[0]) return 0;
    char *buf = strdup(s);
    if (!buf) return -1;
    uint16_t caids[MAX_ECMKEYS_PER_ACC]; int n = 0;
    char *save = NULL, *line = strtok_r(buf, "\r\n;", &save);
    while (line) {
        webif_trim(line);
        if (line[0]) {
            if (n >= MAX_ECMKEYS_PER_ACC) { free(buf); return -1; }
            char *eq = strchr(line, '=');
            if (!eq || eq == line || strlen(eq + 1) != 64) { free(buf); return -1; }
            *eq++ = 0;
            char *endp = NULL; unsigned long caid = strtoul(line, &endp, 16);
            if (endp == line || *endp || caid > 0xFFFF || strlen(line) > 4) { free(buf); return -1; }
            for (int i = 0; i < 64; i++) if (!isxdigit((unsigned char)eq[i])) { free(buf); return -1; }
            for (int i = 0; i < n; i++) if (caids[i] == (uint16_t)caid) { free(buf); return -1; }
            caids[n++] = (uint16_t)caid;
        }
        line = strtok_r(NULL, "\r\n;", &save);
    }
    free(buf);
    return 0;
}

static const char *parse_reader_form(const char *body, reader_form *f, int index)
{
    (void)index;
    memset(f, 0, sizeof(*f));
    if (webif_form_copy(body, "label", f->label, sizeof(f->label)) < 0 ||
        webif_form_copy(body, "protocol", f->protocol, sizeof(f->protocol)) < 0 ||
        webif_form_copy(body, "enabled", f->enabled, sizeof(f->enabled)) < 0 ||
        webif_form_copy(body, "device", f->device, sizeof(f->device)) < 0 ||
        webif_form_copy(body, "user", f->user, sizeof(f->user)) < 0 ||
        webif_form_copy(body, "password", f->password, sizeof(f->password)) < 0 ||
        webif_form_copy(body, "key", f->key, sizeof(f->key)) < 0 ||
        webif_form_copy(body, "inactivitytimeout", f->inactivity, sizeof(f->inactivity)) < 0 ||
        webif_form_copy(body, "caid", f->caid, sizeof(f->caid)) < 0 ||
        webif_form_copy(body, "sid_whitelist", f->sid, sizeof(f->sid)) < 0 ||
        webif_form_copy(body, "ecmwhitelist", f->ecmwl, sizeof(f->ecmwl)) < 0 ||
        webif_form_copy(body, "group", f->groups, sizeof(f->groups)) < 0 ||
        webif_form_copy(body, "ecmkeys", f->ecmkeys, sizeof(f->ecmkeys)) < 0 ||
        webif_form_copy(body, "DO_ECM", f->do_ecm, sizeof(f->do_ecm)) < 0 ||
        webif_form_copy(body, "FAST_RESET", f->fast_reset, sizeof(f->fast_reset)) < 0 ||
        webif_form_copy(body, "POLL_MS", f->poll_ms, sizeof(f->poll_ms)) < 0)
        return "field too long";

    if (!f->label[0]) return "label is required";
    if (!web_valid_text(f->label) || (f->device[0] && !web_valid_text(f->device)) ||
        (f->user[0] && !web_valid_text(f->user)) || (f->password[0] && !web_valid_text(f->password)))
        return "invalid text field";
    const S_READER_PROTOCOL *protocol = reader_protocol_find(f->protocol);
    if (!protocol) return "unsupported reader protocol";

    long v;
    if (rnum(f->enabled, 0, 1, &v) < 0) return "enabled must be 0 or 1";
    if (!f->ecmwl[0]) return "ecmwhitelist is required";
    {
        char *e = NULL; unsigned long x = strtoul(f->ecmwl, &e, 16);
        if (e == f->ecmwl || *e || x > 0xFF) return "ecmwhitelist must be 00-FF";
    }
    if (validate_u16_list(f->caid, MAX_CAIDS_PER_READER, 256) < 0) return "invalid CAID list";
    if (validate_u16_list(f->sid, MAX_SID_WHITELIST, 1024) < 0) return "invalid SID whitelist";
    if (validate_groups(f->groups) < 0) return "group is required";

    if (protocol->kind == READER_PROTOCOL_EMU) {
        if (validate_ecmkeys(f->ecmkeys) < 0) return "invalid ECM key list";
    } else if (protocol->kind == READER_PROTOCOL_CARD) {
        if (rnum(f->do_ecm, 0, 1, &v) < 0) return "DO_ECM must be 0 or 1";
        if (!strcmp(protocol->name, "internal")) {
            if (rnum(f->fast_reset, 0, 86400, &v) < 0) return "FAST_RESET must be 0-86400 seconds for internal readers";
        } else {
            if (rnum(f->fast_reset, 0, 86400, &v) < 0) return "FAST_RESET must be 0-86400";
            if (rnum(f->poll_ms, 50, 10000, &v) < 0) return "POLL_MS must be 50-10000";
        }
        if (!f->device[0]) return "device is required";
    } else {
        if (!f->device[0]) return "server is required";
        if (rnum(f->inactivity, 1, 600, &v) < 0) return "inactivitytimeout must be 1-600";
        if (!strcmp(protocol->name, "newcamd")) {
            if (!f->user[0]) return "username is required";
            if (strlen(f->key) != 28) return "key must be exactly 28 hex characters";
            for (size_t i = 0; i < 28; i++) if (!isxdigit((unsigned char)f->key[i])) return "key must be hexadecimal";
        }
    }
    return NULL;
}

static int view_json(char **dst, int *bsz, int pos, const S_WEBIF_READER_VIEW *r, int detail)
{
    const S_READER_PROTOCOL *protocol = reader_protocol_find(r->protocol);
    const char *kind = protocol ?
        (protocol->kind == READER_PROTOCOL_CARD ? "card" :
         protocol->kind == READER_PROTOCOL_EMU ? "emu" : "network") : "other";
    pos = buf_printf(dst, bsz, pos, "\"index\":%d,\"label\":\"", r->index);
    pos = buf_json_string(dst, bsz, pos, r->label);
    if (pos < 0) return -1;
    pos = buf_printf(dst, bsz, pos, "\",\"protocol\":\"");
    pos = buf_json_string(dst, bsz, pos, r->protocol);
    if (pos < 0) return -1;
    pos = buf_printf(dst, bsz, pos, "\",\"kind\":\"%s\",\"enabled\":%d,\"device\":\"",
                     kind, r->enabled);
    pos = buf_json_string(dst, bsz, pos, r->device);
    if (pos < 0) return -1;
    pos = buf_printf(dst, bsz, pos, "\",\"groups\":\"");
    pos = buf_json_string(dst, bsz, pos, r->groups);
    if (pos < 0) return -1;
    pos = buf_printf(dst, bsz, pos, "\",\"caid\":\"");
    pos = buf_json_string(dst, bsz, pos, r->caids);
    if (pos < 0) return -1;
    int owned = 0;
    int present = 0;
    int ready = 0;
    if (!strcasecmp(r->protocol, "internal")) {
        S_INTERNAL_READER ir;
        if (internal_reader_get(r->index, &ir) == 0) {
            owned = ir.owned;
            present = ir.present;
            ready = ir.ready;
        }
    }
    if (!strcasecmp(r->protocol, "internal")) {
        pos = buf_printf(dst, bsz, pos,
            "\",\"DO_ECM\":%d,\"FAST_RESET\":%d,\"cw_ok\":%lld,\"cw_nok\":%lld,\"active\":%d,\"owned\":%d,\"present\":%d,\"ready\":%d",
            r->do_ecm, r->fast_reset, (long long)r->cw_ok, (long long)r->cw_nok, r->active, owned, present, ready);
    } else {
        pos = buf_printf(dst, bsz, pos,
            "\",\"DO_ECM\":%d,\"FAST_RESET\":%d,\"POLL_MS\":%d,\"cw_ok\":%lld,\"cw_nok\":%lld,\"active\":%d,\"owned\":%d,\"present\":%d,\"ready\":%d",
            r->do_ecm, r->fast_reset, r->poll_ms, (long long)r->cw_ok, (long long)r->cw_nok, r->active, owned, present, ready);
    }
    if (!detail) return pos;

    pos = buf_printf(dst, bsz, pos, ",\"user\":\"");
    pos = buf_json_string(dst, bsz, pos, r->user);
    if (pos < 0) return -1;
    pos = buf_printf(dst, bsz, pos, "\",\"password\":\"");
    pos = buf_json_string(dst, bsz, pos, r->password);
    if (pos < 0) return -1;
    pos = buf_printf(dst, bsz, pos, "\",\"key\":\"");
    pos = buf_json_string(dst, bsz, pos, r->key);
    if (pos < 0) return -1;
    pos = buf_printf(dst, bsz, pos, "\",\"inactivitytimeout\":%d,\"sid_whitelist\":\"", r->inactivitytimeout);
    pos = buf_json_string(dst, bsz, pos, r->sid_whitelist);
    if (pos < 0) return -1;
    pos = buf_printf(dst, bsz, pos, "\",\"ecmwhitelist\":\"%02X\",\"group\":\"",
                     (unsigned)(r->ecm_whitelist & 255));
    pos = buf_json_string(dst, bsz, pos, r->groups);
    if (pos < 0) return -1;
    pos = buf_printf(dst, bsz, pos, "\",\"ecmkeys\":\"");
    pos = buf_json_string(dst, bsz, pos, r->ecmkeys);
    if (pos < 0) return -1;
    return buf_printf(dst, bsz, pos, "\"");
}

void send_api_readers(int fd)
{
    int bsz = 8192, pos = 0;
    char *buf = malloc((size_t)bsz);
    S_WEBIF_READER_VIEW *reader = calloc(1, sizeof(*reader));
    if (!buf || !reader) {
        free(buf); free(reader);
        send_json_error(fd, 503, "Service Unavailable", "out of memory");
        return;
    }
    pos = buf_printf(&buf, &bsz, pos, "{\"ok\":true,\"count\":%d,\"readers\":[", webif_reader_count());
    int emitted = 0;
    for (int idx = 0; idx < MAX_READERS; idx++) {
        if (!webif_reader_get(idx, reader)) continue;
        int before = pos;
        if (emitted) pos = buf_printf(&buf, &bsz, pos, ",");
        pos = buf_printf(&buf, &bsz, pos, "{");
        pos = view_json(&buf, &bsz, pos, reader, 0);
        if (pos >= 0) pos = buf_printf(&buf, &bsz, pos, "}");
        if (pos < 0) {
            pos = before;
            free(reader); free(buf);
            send_json_error(fd, 503, "Service Unavailable", "out of memory");
            return;
        }
        emitted++;
    }
    pos = buf_printf(&buf, &bsz, pos, "]}");
    send_response(fd, 200, "OK", "application/json", buf, pos);
    free(reader); free(buf);
}

void send_api_reader_get(int fd, const char *qs)
{
    char s[16]=""; get_param(qs,"index",s,sizeof(s)); long idx;
    if (rnum(s,0,MAX_READERS-1,&idx)<0) { send_json_error(fd,400,"Bad Request","invalid index"); return; }
    S_WEBIF_READER_VIEW r;
    if (!webif_reader_get((int)idx,&r)) { send_json_error(fd,404,"Not Found","reader not found"); return; }
    int bsz = 8192, pos = 0;
    char *out = malloc((size_t)bsz);
    if (!out) { send_json_error(fd, 503, "Service Unavailable", "out of memory"); return; }
    pos = buf_printf(&out, &bsz, pos, "{\"ok\":true,");
    pos = view_json(&out, &bsz, pos, &r, 1);
    if (pos >= 0) pos = buf_printf(&out, &bsz, pos, "}");
    if (pos < 0) {
        free(out);
        send_json_error(fd, 503, "Service Unavailable", "out of memory");
        return;
    }
    send_response(fd, 200, "OK", "application/json", out, pos);
    free(out);
}

static void form_to_edit(const reader_form *f, int index, S_WEBIF_READER_EDIT *e)
{
    memset(e,0,sizeof(*e)); e->index=index;
    tcmg_strlcpy(e->label,f->label,sizeof(e->label)); tcmg_strlcpy(e->protocol,f->protocol,sizeof(e->protocol));
    tcmg_strlcpy(e->enabled,f->enabled,sizeof(e->enabled)); tcmg_strlcpy(e->device,f->device,sizeof(e->device));
    tcmg_strlcpy(e->user,f->user,sizeof(e->user)); tcmg_strlcpy(e->password,f->password,sizeof(e->password));
    tcmg_strlcpy(e->key,f->key,sizeof(e->key)); tcmg_strlcpy(e->inactivitytimeout,f->inactivity,sizeof(e->inactivitytimeout));
    tcmg_strlcpy(e->caid,f->caid,sizeof(e->caid)); tcmg_strlcpy(e->sid_whitelist,f->sid,sizeof(e->sid_whitelist));
    tcmg_strlcpy(e->ecmwhitelist,f->ecmwl,sizeof(e->ecmwhitelist)); tcmg_strlcpy(e->group,f->groups,sizeof(e->group));
    tcmg_strlcpy(e->ecmkeys,f->ecmkeys,sizeof(e->ecmkeys));
    tcmg_strlcpy(e->do_ecm,f->do_ecm,sizeof(e->do_ecm)); tcmg_strlcpy(e->fast_reset,f->fast_reset,sizeof(e->fast_reset));
    tcmg_strlcpy(e->poll_ms,f->poll_ms,sizeof(e->poll_ms));
}

void handle_api_reader_save(int fd, const char *body)
{
    char idxs[16]=""; webif_form_copy(body,"index",idxs,sizeof(idxs)); long idx=-1;
    int is_new = 0;
    if (idxs[0] && rnum(idxs,-1,MAX_READERS-1,&idx)<0) { send_json_error(fd,400,"Bad Request","invalid index"); return; }
    if (idx < 0) {
        int slot = -1;
        if (!webif_reader_first_free(&slot)) { send_json_error(fd,409,"Conflict","reader limit reached"); return; }
        idx = slot;
        is_new = 1;
    }
    if (!is_new) {
        S_WEBIF_READER_VIEW unused;
        if (!webif_reader_get((int)idx,&unused)) { send_json_error(fd,404,"Not Found","reader not found"); return; }
    }
    reader_form f; const char *err=parse_reader_form(body,&f,(int)idx);
    if (err) { send_json_error(fd,400,"Bad Request",err); return; }
    S_WEBIF_READER_EDIT e; form_to_edit(&f,(int)idx,&e);
    if (!webif_reader_save(&e)) { send_json_error(fd,500,"Internal Error","failed to save config"); return; }
    send_json_ok(fd,"ok");
}

void handle_api_reader_delete(int fd, const char *qs)
{
    char s[16]=""; get_param(qs,"index",s,sizeof(s)); long idx;
    if (rnum(s,0,MAX_READERS-1,&idx)<0) { send_json_error(fd,400,"Bad Request","invalid index"); return; }
    if (!webif_reader_delete((int)idx)) { send_json_error(fd,404,"Not Found","reader not found"); return; }
    send_json_ok(fd,"ok");
}

void handle_api_reader_toggle(int fd, const char *qs)
{
    char s[16] = "";
    get_param(qs, "index", s, sizeof(s));
    long idx;
    if (rnum(s, 0, MAX_READERS - 1, &idx) < 0) {
        send_json_error(fd, 400, "Bad Request", "invalid index");
        return;
    }
    int enabled = 0;
    if (!webif_reader_toggle((int)idx, &enabled)) {
        send_json_error(fd, 404, "Not Found", "reader not found");
        return;
    }
    char out[64];
    int n = snprintf(out, sizeof(out), "{\"ok\":true,\"enabled\":%d}", enabled);
    send_response(fd, 200, "OK", "application/json", out, n);
}

void send_api_serial_ports(int fd)
{
    char ports[TCMG_SERIAL_MAX_PORTS][TCMG_SERIAL_PORT_LEN];
    size_t n = serial_list_ports(ports, TCMG_SERIAL_MAX_PORTS);
    size_t cap = 4096 + n * (TCMG_SERIAL_PORT_LEN + 64);
    char *buf = malloc(cap);
    if (!buf) {
        send_json_error(fd, 503, "Service Unavailable", "out of memory");
        return;
    }
    int pos = snprintf(buf, cap, "{\"ok\":true,\"count\":%zu,\"transport\":\"serial\",\"ports\":[", n);
    for (size_t i = 0; i < n && pos >= 0 && (size_t)pos < cap; i++) {
        char esc[TCMG_SERIAL_PORT_LEN * 2 + 8];
        json_escape(ports[i], esc, sizeof(esc));
        pos += snprintf(buf + pos, cap - (size_t)pos, "%s\"%s\"", i ? "," : "", esc);
    }
    if (pos >= 0 && (size_t)pos < cap) pos += snprintf(buf + pos, cap - (size_t)pos, "]}");
    if (pos < 0 || (size_t)pos >= cap) {
        free(buf);
        send_json_error(fd, 500, "Internal Server Error", "serial port response too large");
        return;
    }
    send_response(fd, 200, "OK", "application/json", buf, pos);
    free(buf);
}
