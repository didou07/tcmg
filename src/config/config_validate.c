#define MODULE_LOG_PREFIX "conf"
#include "config_internal.h"

void cfg_path_sibling(char *out,size_t outsz,const char *global,const char *name)
{
    char dir[CFGPATH_LEN];tcmg_strlcpy(dir,global,sizeof(dir));char*p1=strrchr(dir,'/'),*p2=strrchr(dir,'\\'),*p=p1>p2?p1:p2;if(p)*p='\0';else tcmg_strlcpy(dir,".",sizeof(dir));tcmg_build_path(out,outsz,dir,name);
}

bool cfg_validate(S_CONFIG *c, char *err, size_t esz)
{
    int ports[4];
    int port_count = 0;

    if (!c) {
        snprintf(err, esz, "null configuration");
        return false;
    }

    if (c->newcamd_port) ports[port_count++] = c->newcamd_port;
    if (c->cccam_port) ports[port_count++] = c->cccam_port;
    if (c->cs378x_port) ports[port_count++] = c->cs378x_port;
    if (c->webif_enabled && c->webif_port) ports[port_count++] = c->webif_port;

    for (int i = 0; i < port_count; i++) {
        for (int j = i + 1; j < port_count; j++) {
            if (ports[i] == ports[j]) {
                snprintf(err, esz, "port conflict on %d", ports[i]);
                return false;
            }
        }
    }

    for (S_ACCOUNT *a = c->accounts; a; a = a->next) {
        if (!a->user[0]) {
            snprintf(err, esz, "account without user");
            return false;
        }
        if (!a->ngroups || a->ngroups > MAX_GROUPS_PER_ACC) {
            snprintf(err, esz, "account '%s' has invalid groups", a->user);
            return false;
        }
        if (a->caid == 0 && a->ncaids > 0) {
            snprintf(err, esz, "account '%s': caid=0000 cannot be mixed with specific CAIDs", a->user);
            return false;
        }
        for (S_ACCOUNT *b = a->next; b; b = b->next) {
            if (!strcmp(a->user, b->user)) {
                snprintf(err, esz, "duplicate account user '%s'", a->user);
                return false;
            }
        }
    }

    for (int i = 0; i < MAX_READERS; i++) {
        S_READER *r = &c->readers[i];
        if (!r->in_use) continue;

        if (!r->label[0]) {
            snprintf(err, esz, "reader[%d] has empty label", i);
            return false;
        }
        if (!r->ngroups || r->ngroups > MAX_GROUPS_PER_READER) {
            snprintf(err, esz, "reader[%d] '%s' has invalid groups", i, r->label);
            return false;
        }
        if (r->ncaids > MAX_CAIDS_PER_READER || r->nsid_whitelist > MAX_SID_WHITELIST) {
            snprintf(err, esz, "reader[%d] '%s' has invalid list size", i, r->label);
            return false;
        }

        if (!strcasecmp(r->protocol, "emu")) {
            if (r->enabled && r->nkeys == 0) {
                snprintf(err, esz, "reader[%d] '%s': enabled emu reader requires ecmkey", i, r->label);
                return false;
            }
        }
        else if (!strcasecmp(r->protocol, "pcsc")) {
            if (r->enabled && !r->device[0]) {
                snprintf(err, esz, "reader[%d] '%s': PCSC reader requires a device/reader selector", i, r->label);
                return false;
            }
            if (r->fast_reset < 0 || r->fast_reset > 86400 ||
                r->poll_ms < 25 || r->poll_ms > 10000) {
                snprintf(err, esz, "reader[%d] '%s': invalid PCSC timing", i, r->label);
                return false;
            }
        }
        else if (!strcasecmp(r->protocol, "internal")) {
            if (r->enabled && !r->device[0]) {
                snprintf(err, esz, "reader[%d] '%s': device is required", i, r->label);
                return false;
            }
            if (r->fast_reset < 0 || r->fast_reset > 86400 ||
                r->poll_ms < 25 || r->poll_ms > 10000) {
                snprintf(err, esz, "reader[%d] '%s': invalid internal timing", i, r->label);
                return false;
            }
        }
        else if (!strcasecmp(r->protocol, "cccam") ||
                 !strcasecmp(r->protocol, "cs378x") ||
                 !strcasecmp(r->protocol, "newcamd") ||
                 !strcasecmp(r->protocol, "mgcamd")) {
            const char *comma;
            long port;

            if (!r->enabled) continue;
            if (!r->device[0]) {
                snprintf(err, esz, "reader[%d] '%s': device is required", i, r->label);
                return false;
            }

            comma = strrchr(r->device, ',');
            if (!comma || comma == r->device || !comma[1]) {
                snprintf(err, esz, "reader[%d] '%s': invalid device '%s' (expected host,port)",
                         i, r->label, r->device);
                return false;
            }
            if (!cfg_parse_long_range(comma + 1, 1, 65535, &port)) {
                snprintf(err, esz, "reader[%d] '%s': invalid port", i, r->label);
                return false;
            }

            if (!strcasecmp(r->protocol, "newcamd") || !strcasecmp(r->protocol, "mgcamd")) {
                bool nonzero = false;
                for (size_t j = 0; j < sizeof(r->newcamd_key); j++) {
                    if (r->newcamd_key[j]) {
                        nonzero = true;
                        break;
                    }
                }
                if (!nonzero) {
                    snprintf(err, esz, "reader[%d] '%s': key is required", i, r->label);
                    return false;
                }
                if (!r->user[0]) {
                    snprintf(err, esz, "reader[%d] '%s': user is required", i, r->label);
                    return false;
                }
            }
        }
        else {
            snprintf(err, esz, "reader[%d] '%s': unsupported protocol '%s'", i, r->label, r->protocol);
            return false;
        }
    }

    return true;
}

bool cfg_listener_settings_changed(const S_CONFIG *old_cfg,
                                          const S_CONFIG *new_cfg,
                                          char *err, size_t errsz)
{
    if (old_cfg->webif_enabled != new_cfg->webif_enabled ||
        old_cfg->webif_port != new_cfg->webif_port ||
        strcmp(old_cfg->webif_bindaddr, new_cfg->webif_bindaddr) != 0) {
        snprintf(err, errsz,
                 "webif listener changed; restart TCMG to apply port/bind changes");
        return true;
    }
    if (old_cfg->newcamd_port != new_cfg->newcamd_port ||
        strcmp(old_cfg->newcamd_bindaddr, new_cfg->newcamd_bindaddr) != 0) {
        snprintf(err, errsz,
                 "newcamd listener changed; restart TCMG to apply port/bind changes");
        return true;
    }
    if (old_cfg->cccam_port != new_cfg->cccam_port ||
        strcmp(old_cfg->cccam_bindaddr, new_cfg->cccam_bindaddr) != 0) {
        snprintf(err, errsz,
                 "cccam listener changed; restart TCMG to apply port/bind changes");
        return true;
    }
    if (old_cfg->cs378x_port != new_cfg->cs378x_port ||
        strcmp(old_cfg->cs378x_bindaddr, new_cfg->cs378x_bindaddr) != 0) {
        snprintf(err, errsz,
                 "cs378x listener changed; restart TCMG to apply port/bind changes");
        return true;
    }
    return false;
}

