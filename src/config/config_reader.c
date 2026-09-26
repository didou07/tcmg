#define MODULE_LOG_PREFIX "conf"
#include "config_internal.h"
#include "../reader/protocol.h"

S_READER *cfg_reader_new(S_CONFIG *cfg, int index)
{
    if (!cfg || index < 0 || index >= MAX_READERS) return NULL;
    S_READER *r = &cfg->readers[index];
    if (!r->in_use) {
        memset(r, 0, sizeof(*r));
        r->in_use = 1;
        r->enabled = 1;
        r->inactivitytimeout = 30;
        r->ecm_whitelist = 255;
        r->do_ecm = 1;
        r->ngroups = 1;
        r->groups[0] = 1;
        tcmg_strlcpy(r->protocol, "emu", sizeof(r->protocol));
        cfg->nreaders++;
    }
    return r;
}
bool cfg_parse_readers(const char *path, S_CONFIG *c, char *err, size_t esz)
{
    FILE *f = fopen(path, "r");
    char line[4096];
    char sec[64] = "";
    char key[128];
    bool used[MAX_READERS] = { false };
    int line_no = 0;
    S_READER *reader = NULL;

    if (!f) {
        if (errno == ENOENT) return true;
        snprintf(err, esz, "%s: %s", path, strerror(errno));
        return false;
    }

    while (fgets(line, sizeof(line), f)) {
        line_no++;
        if (!strchr(line, '\n') && !feof(f)) {
            snprintf(err, esz, "%s:%d: line too long", path, line_no);
            fclose(f);
            return false;
        }

        cfg_str_trim(line);
        if (!line[0] || line[0] == '#') continue;

        if (line[0] == '[') {
            char *rr = strrchr(line, ']');
            int index = -1;

            if (!rr || rr == line + 1 || rr[1]) goto badline;
            *rr = '\0';
            if (strlen(line + 1) >= sizeof(sec)) goto badline;
            tcmg_strlcpy(sec, line + 1, sizeof(sec));

            if (strcasecmp(sec, "reader") != 0) {
                snprintf(err, esz, "%s:%d: only [reader] sections are allowed", path, line_no);
                fclose(f);
                return false;
            }

            for (int i = 0; i < MAX_READERS; i++) {
                if (!used[i]) {
                    index = i;
                    break;
                }
            }
            if (index < 0) {
                snprintf(err, esz, "%s:%d: maximum number of readers (%d) reached",
                         path, line_no, MAX_READERS);
                fclose(f);
                return false;
            }

            used[index] = true;
            reader = cfg_reader_new(c, index);
            if (!reader) {
                snprintf(err, esz, "%s:%d: cannot allocate reader", path, line_no);
                fclose(f);
                return false;
            }
            continue;
        }

        if (!reader) {
            snprintf(err, esz, "%s:%d: key before [reader]", path, line_no);
            fclose(f);
            return false;
        }

        {
            char *eq = strchr(line, '=');
            char value[CFGVAL_LEN];
            long v;
            int b;

            if (!eq) goto badline;
            *eq = '\0';
            cfg_str_trim(line);
            if (!line[0] || strlen(line) >= sizeof(key)) goto badline;
            cfg_strip_inline_comment(eq + 1);
            if (strlen(eq + 1) >= sizeof(value)) goto badval;
            tcmg_strlcpy(key, line, sizeof(key));
            tcmg_strlcpy(value, eq + 1, sizeof(value));

            if (!strcasecmp(key, "label")) {
                if (strlen(value) >= sizeof(reader->label)) goto badval;
                tcmg_strlcpy(reader->label, value, sizeof(reader->label));
            }
            else if (!strcasecmp(key, "protocol")) {
                if (!reader_protocol_find(value)) goto badval;
                tcmg_strlcpy(reader->protocol, value, sizeof(reader->protocol));
            }
            else if (!strcasecmp(key, "enabled")) {
                if (!cfg_parse_bool(value, &b)) goto badval;
                reader->enabled = (int8_t)b;
            }
            else if (!strcasecmp(key, "device")) {
                if (strlen(value) >= sizeof(reader->device)) goto badval;
                tcmg_strlcpy(reader->device, value, sizeof(reader->device));
            }
            else if (!strcasecmp(key, "user")) {
                if (strlen(value) >= sizeof(reader->user)) goto badval;
                tcmg_strlcpy(reader->user, value, sizeof(reader->user));
            }
            else if (!strcasecmp(key, "password")) {
                if (strlen(value) >= sizeof(reader->password)) goto badval;
                tcmg_strlcpy(reader->password, value, sizeof(reader->password));
            }
            else if (!strcasecmp(key, "group")) {
                if (!cfg_parse_group_list(value, reader->groups, &reader->ngroups)) goto badval;
            }
            else if (!strcasecmp(key, "caid")) {
                if (!cfg_parse_u16_list(value, reader->caids, &reader->ncaids,
                                    MAX_CAIDS_PER_READER)) goto badval;
            }
            else if (!strcasecmp(key, "sid_whitelist")) {
                if (!cfg_parse_u16_list(value, reader->sid_whitelist, &reader->nsid_whitelist,
                                    MAX_SID_WHITELIST)) goto badval;
            }
            else if (!strcasecmp(key, "ecm_maxlen")) {
                if (!cfg_parse_long_range(value, 0, 255, &v)) goto badval;
                reader->ecm_whitelist = (int32_t)v;
            }
            else if (!strcasecmp(key, "inactivitytimeout")) {
                if (!cfg_parse_long_range(value, 1, 600, &v)) goto badval;
                reader->inactivitytimeout = (int32_t)v;
            }
            else if (!strcasecmp(key, "do_ecm")) {
                if (!cfg_parse_bool(value, &b)) goto badval;
                reader->do_ecm = (int8_t)b;
            }
            else if (!strcasecmp(key, "fast_reset")) {
                if (!cfg_parse_long_range(value, 0, 86400, &v)) goto badval;
                reader->fast_reset = (int32_t)v;
            }
            else if (!strcasecmp(key, "poll_ms")) {
                if (!cfg_parse_long_range(value, 25, 10000, &v)) goto badval;
                reader->poll_ms = (int32_t)v;
            }
            else if (!strcasecmp(key, "key")) {
                if (!cfg_parse_hex_bytes(value, reader->newcamd_key, 14)) goto badval;
            }
            else if (!strcasecmp(key, "ecmkey")) {
                uint16_t def_caid = reader->ncaids ? reader->caids[0] : 0;
                S_ECMKEY key_value;
                bool found = false;

                if (reader->nkeys >= MAX_ECMKEYS_PER_ACC ||
                    !cfg_parse_ecm_key(value, def_caid, &key_value)) goto badval;
                for (int i = 0; i < reader->nkeys; i++) {
                    if (reader->keys[i].caid == key_value.caid) {
                        reader->keys[i] = key_value;
                        found = true;
                        break;
                    }
                }
                if (!found) reader->keys[reader->nkeys++] = key_value;
            }
            else {
                snprintf(err, esz, "%s:%d: unknown key '%s'", path, line_no, key);
                fclose(f);
                return false;
            }
        }
    }

    fclose(f);

    for (int i = 0; i < MAX_READERS; i++) {
        reader = &c->readers[i];
        if (!reader->in_use) continue;
        if (!reader->label[0])
            snprintf(reader->label, sizeof(reader->label), "reader%d", i + 1);
    }
    return true;

badval:
    snprintf(err, esz, "%s:%d: invalid value for '%s'", path, line_no, key);
    fclose(f);
    return false;
badline:
    snprintf(err, esz, "%s:%d: invalid line", path, line_no);
    fclose(f);
    return false;
}
