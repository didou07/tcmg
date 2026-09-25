#define MODULE_LOG_PREFIX "conf"
#include "config_internal.h"

#if defined(TCMG_OS_WINDOWS)
#  include <process.h>
static unsigned long legacy_pid(void) { return (unsigned long)_getpid(); }
#else
static unsigned long legacy_pid(void) { return (unsigned long)getpid(); }
#endif

typedef enum {
    LEG_NONE,
    LEG_SERVER,
    LEG_WEBIF,
    LEG_PCSC,
    LEG_MGCAMD,
    LEG_ACCOUNT,
    LEG_READER
} legacy_sec;

static bool legacy_scratch_path(char *out, size_t outsz, const char *dir, unsigned long pid, const char *suffix)
{
    int n = snprintf(out, outsz, "%s/.tcmg-legacy-%lu.%s", dir, pid, suffix);
    return n >= 0 && (size_t)n < outsz;
}

static void legacy_dirname(const char *path, char *out, size_t outsz)
{
    char tmp[CFGPATH_LEN];
    char *p1, *p2, *p;
    tcmg_strlcpy(tmp, path, sizeof(tmp));
    p1 = strrchr(tmp, '/');
    p2 = strrchr(tmp, '\\');
    p = p1 > p2 ? p1 : p2;
    if (p) {
        *p = '\0';
        if (!tmp[0]) tcmg_strlcpy(tmp, ".", sizeof(tmp));
    } else {
        tcmg_strlcpy(tmp, ".", sizeof(tmp));
    }
    tcmg_strlcpy(out, tmp, outsz);
}

static bool legacy_is_section(const char *line, char *name, size_t nsz)
{
    size_t n;
    const char *end;
    if (!line || line[0] != '[') return false;
    end = strrchr(line, ']');
    if (!end || end <= line + 1 || end[1]) return false;
    n = (size_t)(end - line - 1);
    if (n >= nsz) return false;
    memcpy(name, line + 1, n);
    name[n] = '\0';
    return true;
}

static bool legacy_detect_cb_section(const char *sec)
{
    if (!sec) return false;
    if (!strcasecmp(sec, "server") || !strcasecmp(sec, "webif") ||
        !strcasecmp(sec, "pcsc") || !strcasecmp(sec, "mgcamd") ||
        !strcasecmp(sec, "account") || !strcasecmp(sec, "reader")) return true;
    if (!strncasecmp(sec, "reader", 6)) {
        const char *p = sec + 6;
        if (*p) {
            char *e = NULL;
            (void)strtol(p, &e, 10);
            if (e && *e == '\0') return true;
        }
    }
    return false;
}

bool cfg_legacy_detect(const char *path)
{
    FILE *f;
    char line[4096];
    char sec[128];
    if (!path || !*path) return false;
    f = fopen(path, "r");
    if (!f) return false;
    while (fgets(line, sizeof(line), f)) {
        cfg_str_trim(line);
        if (!line[0] || line[0] == '#') continue;
        if (!legacy_is_section(line, sec, sizeof(sec))) {
            fclose(f);
            return false;
        }
        fclose(f);
        return legacy_detect_cb_section(sec);
    }
    fclose(f);
    return false;
}

static bool legacy_copy_with_backup(const char *src, const char *backup)
{
    FILE *in = fopen(src, "rb");
    char buf[8192];
    size_t n;
    char *data = NULL;
    size_t len = 0, cap = 0;
    bool ok = false;

    if (!in) return false;
    while ((n = fread(buf, 1, sizeof(buf), in)) != 0) {
        if (len + n + 1 > cap) {
            size_t nc = cap ? cap * 2 : 16384;
            while (nc < len + n + 1) nc *= 2;
            char *nd = realloc(data, nc);
            if (!nd) goto done;
            data = nd;
            cap = nc;
        }
        memcpy(data + len, buf, n);
        len += n;
    }
    if (ferror(in)) goto done;
    if (!data) {
        data = calloc(1, 1);
        if (!data) goto done;
    }
    data[len] = '\0';
    ok = cfg_write_atomic(backup, data);

done:
    free(data);
    fclose(in);
    return ok;
}

static bool write_line(FILE *f, const char *section, const char *key, const char *value,
                       char *last_section, size_t last_sz)
{
    if (strcmp(last_section, section) != 0) {
        if (fprintf(f, "[%s]\n", section) < 0) return false;
        tcmg_strlcpy(last_section, section, last_sz);
    }
    return fprintf(f, "%s = %s\n", key, value ? value : "") >= 0;
}

static bool map_server(const char *key, const char *value, FILE *global, char *last, size_t last_sz)
{
    const char *sec = NULL, *out = NULL;
    if (!strcasecmp(key, "NEWCAMD_PORT"))      { sec = "newcamd"; out = "port"; }
    else if (!strcasecmp(key, "NEWCAMD_BINDADDR")) { sec = "newcamd"; out = "bindaddr"; }
    else if (!strcasecmp(key, "NEWCAMD_KEY"))   { sec = "newcamd"; out = "key"; }
    else if (!strcasecmp(key, "NEWCAMD_KEEPALIVE")) { sec = "newcamd"; out = "keepalive"; }
    else if (!strcasecmp(key, "NEWCAMD_MGCLIENT")) {
        return write_line(global, "newcamd", "mode", (!strcasecmp(value, "1") || !strcasecmp(value, "yes") || !strcasecmp(value, "true")) ? "mgcamd" : "auto", last, last_sz);
    }
    else if (!strcasecmp(key, "CCCAM_PORT"))    { sec = "cccam"; out = "port"; }
    else if (!strcasecmp(key, "CS378X_PORT"))   { sec = "cs378x"; out = "port"; }
    else if (!strcasecmp(key, "CS378X_BINDADDR")) { sec = "cs378x"; out = "bindaddr"; }
    else if (!strcasecmp(key, "SOCKET_TIMEOUT") || !strcasecmp(key, "SOCK_TIMEOUT")) { sec = "global"; out = "socket_timeout"; }
    else if (!strcasecmp(key, "SERVER_KEEPALIVE")) { sec = "global"; out = "server_keepalive"; }
    else if (!strcasecmp(key, "SERVER_KEEPALIVE_MISSES")) { sec = "global"; out = "server_keepalive_misses"; }
    else if (!strcasecmp(key, "ECM_LOG"))       { sec = "global"; out = "ecm_log"; }
    else if (!strcasecmp(key, "LOGFILE"))       { sec = "global"; out = "logfile"; }
    else if (!strcasecmp(key, "USRFILE"))       { sec = "global"; out = "usrfile"; }
    else if (!strcasecmp(key, "FAILBAN_ENABLED")) { sec = "failban"; out = "enabled"; }
    else if (!strcasecmp(key, "FAILBAN_ALLOWLIST")) { sec = "failban"; out = "allowlist"; }
    else if (!strcasecmp(key, "FAILBAN_MAX_FAILS")) { sec = "failban"; out = "max_fails"; }
    else if (!strcasecmp(key, "FAILBAN_BAN_SECS")) { sec = "failban"; out = "ban_secs"; }
    else return true; /* preserve old parser behavior: unsupported keys are ignored */
    return write_line(global, sec, out, value, last, last_sz);
}

static bool map_webif(const char *key, const char *value, FILE *global, char *last, size_t last_sz)
{
    const char *out = key;
    if (!strcasecmp(key, "PWD") || !strcasecmp(key, "PASS") || !strcasecmp(key, "PASSWORD")) out = "password";
    else if (!strcasecmp(key, "USER")) out = "user";
    else if (!strcasecmp(key, "PORT")) out = "port";
    else if (!strcasecmp(key, "REFRESH")) out = "refresh";
    else if (!strcasecmp(key, "ENABLED")) out = "enabled";
    else if (!strcasecmp(key, "BINDADDR")) out = "bindaddr";
    else return true;
    return write_line(global, "webif", out, value, last, last_sz);
}

static bool map_pcsc(const char *key, const char *value, FILE *global, char *last, size_t last_sz)
{
    const char *out = key;
    if (!strcasecmp(key, "ENABLED")) out = "enabled";
    else if (!strcasecmp(key, "FAST_RESET")) out = "fast_reset";
    else if (!strcasecmp(key, "POLL_MS")) out = "poll_ms";
    else if (!strcasecmp(key, "READER")) out = "reader";
    else return true;
    return write_line(global, "pcsc", out, value, last, last_sz);
}

static bool map_reader(const char *key, const char *value, FILE *readers, char *last, size_t last_sz)
{
    const char *out = key;
    if (!strcasecmp(key, "PWD")) out = "password";
    else if (!strcasecmp(key, "ECMWHITELIST")) out = "ecm_maxlen";
    else if (!strcasecmp(key, "DO_ECM")) out = "do_ecm";
    else if (!strcasecmp(key, "FAST_RESET")) out = "fast_reset";
    else if (!strcasecmp(key, "POLL_MS")) out = "poll_ms";
    return write_line(readers, "reader", out, value, last, last_sz);
}

static bool map_key_value(const char *sec, const char *key, const char *value,
                          FILE *global, FILE *users, FILE *readers,
                          char *last_global, char *last_users, char *last_readers,
                          bool *have_user_or_reader, char *err, size_t errsz)
{
    if (!strcasecmp(sec, "server")) return map_server(key, value, global, last_global, 64);
    if (!strcasecmp(sec, "webif")) return map_webif(key, value, global, last_global, 64);
    if (!strcasecmp(sec, "pcsc")) return map_pcsc(key, value, global, last_global, 64);
    if (!strcasecmp(sec, "mgcamd")) return true;
    if (!strcasecmp(sec, "account")) {
        *have_user_or_reader = true;
        return fprintf(users, "%s = %s\n", key, value ? value : "") >= 0;
    }
    if (!strcasecmp(sec, "reader") || !strncasecmp(sec, "reader", 6)) {
        *have_user_or_reader = true;
        return map_reader(key, value, readers, last_readers, sizeof(last_readers));
    }
    (void)last_users;
    if (err && errsz) snprintf(err, errsz, "unknown legacy section [%s]", sec);
    return false;
}

bool cfg_load_legacy(const char *path, S_CONFIG *cfg, char *err, size_t errsz)
{
    FILE *in = NULL, *global = NULL, *users = NULL, *readers = NULL;
    char dir[CFGPATH_LEN], tglob[CFGPATH_LEN], tusers[CFGPATH_LEN], treaders[CFGPATH_LEN];
    char backup[CFGPATH_LEN];
    char line[4096], section[128], key[128], value[CFGVAL_LEN];
    char last_global[64] = "", last_user[64] = "", last_reader[64] = "";
    bool have_user_or_reader = false;
    bool ok = false;
    unsigned long pid = legacy_pid();

    if (!path || !cfg || !err || errsz == 0) return false;
    err[0] = '\0';
    legacy_dirname(path, dir, sizeof(dir));
    if (!legacy_scratch_path(tglob, sizeof(tglob), dir, pid, "global") ||
        !legacy_scratch_path(tusers, sizeof(tusers), dir, pid, "users") ||
        !legacy_scratch_path(treaders, sizeof(treaders), dir, pid, "readers")) {
        snprintf(err, errsz, "legacy migration path is too long");
        return false;
    }
    if (snprintf(backup, sizeof(backup), "%s.legacy.bak", path) < 0 ||
        strlen(path) + strlen(".legacy.bak") >= sizeof(backup)) {
        snprintf(err, errsz, "legacy backup path is too long");
        return false;
    }

    in = fopen(path, "r");
    if (!in) { snprintf(err, errsz, "%s: %s", path, strerror(errno)); return false; }
    global = fopen(tglob, "wb");
    users = fopen(tusers, "wb");
    readers = fopen(treaders, "wb");
    if (!global || !users || !readers) {
        snprintf(err, errsz, "cannot create migration scratch files");
        goto done;
    }
    fprintf(global, "# generated from legacy monolithic configuration\n\n");
    fprintf(users, "# generated from legacy monolithic configuration\n\n");
    fprintf(readers, "# generated from legacy monolithic configuration\n\n");

    strcpy(section, "");
    while (fgets(line, sizeof(line), in)) {
        char *eq;
        cfg_str_trim(line);
        if (!line[0] || line[0] == '#') continue;
        if (!legacy_is_section(line, section, sizeof(section))) {
            eq = strchr(line, '=');
            if (!eq) { snprintf(err, errsz, "%s: invalid legacy line", path); goto done; }
            *eq = '\0';
            tcmg_strlcpy(key, line, sizeof(key));
            tcmg_strlcpy(value, eq + 1, sizeof(value));
            cfg_str_trim(key);
            cfg_strip_inline_comment(value);
            if (!key[0]) { snprintf(err, errsz, "%s: invalid legacy key", path); goto done; }
            if (!map_key_value(section, key, value, global, users, readers,
                               last_global, last_user, last_reader,
                               &have_user_or_reader, err, errsz)) goto done;
            continue;
        }
        if (!strcasecmp(section, "server") || !strcasecmp(section, "webif") || !strcasecmp(section, "pcsc") || !strcasecmp(section, "mgcamd")) {
            continue;
        }
        if (!strcasecmp(section, "account")) {
            if (fprintf(users, "[account]\n") < 0) goto done;
            last_user[0] = '\0';
            continue;
        }
        if (!strcasecmp(section, "reader") || !strncasecmp(section, "reader", 6)) {
            if (fprintf(readers, "[reader]\n") < 0) goto done;
            tcmg_strlcpy(last_reader, "reader", sizeof(last_reader));
            continue;
        }
        snprintf(err, errsz, "%s: unknown legacy section [%s]", path, section);
        goto done;
    }
    if (ferror(in)) { snprintf(err, errsz, "%s: read error", path); goto done; }
    fclose(in); in = NULL;
    fclose(global); global = NULL;
    fclose(users); users = NULL;
    fclose(readers); readers = NULL;

    if (have_user_or_reader) {
        /* scratch files can be validated through the new parser below */
    }

    cfg_default_runtime(cfg);
    tcmg_strlcpy(cfg->config_file, path, sizeof(cfg->config_file));
    cfg_path_sibling(cfg->user_file, sizeof(cfg->user_file), path, TCMG_USER_FILE);
    cfg_path_sibling(cfg->server_file, sizeof(cfg->server_file), path, TCMG_SERVER_FILE);

    if (!cfg_parse_file(tglob, cfg_global_cb, cfg, err, errsz)) goto done;
    if (!cfg_parse_users(tusers, cfg, err, errsz)) goto done;
    if (!cfg_parse_readers(treaders, cfg, err, errsz)) goto done;
    if (!cfg_validate(cfg, err, errsz)) goto done;

    /* Preserve the original file before converting it. */
    if (!legacy_copy_with_backup(path, backup)) {
        snprintf(err, errsz, "cannot create legacy backup %s", backup);
        goto done;
    }

    {
        enum { GLOBAL_CAP = 32768, USER_CAP = 262144, READER_CAP = 262144 };
        char *gtext = calloc(1, GLOBAL_CAP);
        char *utext = calloc(1, USER_CAP);
        char *rtext = calloc(1, READER_CAP);
        if (!gtext || !utext || !rtext) {
            free(gtext); free(utext); free(rtext);
            snprintf(err, errsz, "out of memory while migrating configuration");
            goto done;
        }
        if (!cfg_build_global_text(cfg, gtext, GLOBAL_CAP) ||
            !cfg_build_user_text(cfg, utext, USER_CAP) ||
            !cfg_build_server_text(cfg, rtext, READER_CAP)) {
            free(gtext); free(utext); free(rtext);
            snprintf(err, errsz, "configuration is too large to migrate");
            goto done;
        }
        /* Write children first; replace the old monolithic file last. */
        if (!cfg_write_atomic(cfg->user_file, utext) ||
            !cfg_write_atomic(cfg->server_file, rtext) ||
            !cfg_write_atomic(cfg->config_file, gtext)) {
            free(gtext); free(utext); free(rtext);
            snprintf(err, errsz, "cannot write migrated split configuration");
            goto done;
        }
        free(gtext); free(utext); free(rtext);
    }

    tcmg_log("config migrated: legacy %s -> %s + %s + %s (backup=%s)",
             path, cfg->config_file, cfg->user_file, cfg->server_file, backup);
    ok = true;

done:
    if (in) fclose(in);
    if (global) fclose(global);
    if (users) fclose(users);
    if (readers) fclose(readers);
    remove(tglob); remove(tusers); remove(treaders);
    return ok;
}
