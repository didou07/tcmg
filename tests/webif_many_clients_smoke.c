#include "core/config_state.h"
#include "core/runtime_state.h"
#include "core/client_state.h"
#include "config/config.h"
#include "webif/internal/proto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int write_text(const char *path, const char *s)
{
    FILE *f = fopen(path, "wb");
    size_t n = strlen(s);
    if (!f) return 0;
    if (fwrite(s, 1, n, f) != n) { fclose(f); return 0; }
    return fclose(f) == 0;
}

static int init_cfg(const char *dir)
{
    char path[512];
    const char *global =
        "[global]\nsocket_timeout = 30\nserver_keepalive = 20\nserver_keepalive_misses = 3\n"
        "ecm_log = 0\nlogfile =\nusrfile =\n"
        "[webif]\nenabled = 1\nport = 18080\nrefresh = 7\nuser = admin\npassword = secret\nbindaddr = 127.0.0.1\n"
        "[newcamd]\nport = 19050\nbindaddr =\nkey = 0102030405060708091011121314\nkeepalive = 1\nmode = auto\n"
        "[cccam]\nport = 19060\nbindaddr =\n[cs378x]\nport = 19070\nbindaddr =\n"
        "[failban]\nenabled = 0\nallowlist =\nmax_fails = 5\nban_secs = 300\n";
    const char *users =
        "[account]\nuser = admin\npwd = pass\nenabled = 1\ngroup = 1\ncaid = 0B00\nmax_connections = 64\n";
    snprintf(path, sizeof(path), "%s/tcmg.conf", dir);
    if (!write_text(path, global)) return 0;
    snprintf(path, sizeof(path), "%s/tcmg.users", dir);
    if (!write_text(path, users)) return 0;
    snprintf(path, sizeof(path), "%s/tcmg.readers", dir);
    if (!write_text(path, "")) return 0;

    memset(&g_cfg, 0, sizeof(g_cfg));
    if (pthread_rwlock_init(&g_cfg.acc_lock, NULL) != 0) return 0;
    if (pthread_mutex_init(&g_cfg.ban_lock, NULL) != 0) return 0;
    strncpy(g_cfgdir, dir, sizeof(g_cfgdir) - 1);
    g_cfgdir[sizeof(g_cfgdir) - 1] = 0;
    g_start_time = time(NULL) - 10;
    snprintf(path, sizeof(path), "%s/tcmg.conf", dir);
    return cfg_load(path, &g_cfg) ? 1 : 0;
}

static S_CLIENT *g_fake[MAX_ACTIVE_CLIENTS];
static S_ACCOUNT g_fake_acct;

static void set_clients(int n)
{
    pthread_mutex_lock(&g_clients_mtx);
    for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++) g_clients[i] = NULL;
    for (int i = 0; i < n && i < MAX_ACTIVE_CLIENTS; i++) {
        if (!g_fake[i]) g_fake[i] = (S_CLIENT *)calloc(1, sizeof(S_CLIENT));
        S_CLIENT *c = g_fake[i];
        memset(c, 0, sizeof(*c));
        snprintf(c->identity.user, sizeof(c->identity.user), "admin");
        snprintf(c->identity.ip, sizeof(c->identity.ip), "10.%u.%u.%u",
                 (unsigned)((i / 65536) & 255), (unsigned)((i / 256) & 255), (unsigned)(i % 256));
        snprintf(c->protocol.name, sizeof(c->protocol.name), "newcamd");
        snprintf(c->ecm.last_channel, sizeof(c->ecm.last_channel), "Channel %d", i);
        c->ecm.last_caid = 0x0B00;
        c->ecm.last_srvid = (uint16_t)(0x1000 + i);
        c->identity.thread_id = (uint32_t)(1000 + i);
        c->session.connect_time = time(NULL) - 5;
        c->auth.account = &g_fake_acct;
        g_clients[i] = c;
    }
    pthread_mutex_unlock(&g_clients_mtx);
}

static int count_str(const char *hay, const char *needle)
{
    int n = 0;
    size_t l = strlen(needle);
    for (const char *p = hay; (p = strstr(p, needle)); p += l) n++;
    return n;
}

typedef void (*handler_fn)(int fd);
static void h_status(int fd)    { send_api_status(fd, ""); }
static void h_userstats(int fd) { send_api_userstats(fd); }
static void h_users(int fd)     { send_page_users(fd); }

static char *run_handler(handler_fn fn)
{
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return NULL;
    int sz = 1 << 20;
    setsockopt(sv[1], SOL_SOCKET, SO_SNDBUF, &sz, sizeof(sz));
    setsockopt(sv[0], SOL_SOCKET, SO_RCVBUF, &sz, sizeof(sz));
    fn(sv[1]);
    close(sv[1]);
    size_t cap = 1 << 16, len = 0;
    char *out = (char *)malloc(cap);
    for (;;) {
        if (len + 4096 + 1 > cap) { cap *= 2; out = (char *)realloc(out, cap); }
        ssize_t r = read(sv[0], out + len, cap - len - 1);
        if (r <= 0) break;
        len += (size_t)r;
    }
    out[len] = '\0';
    close(sv[0]);
    return out;
}

int main(void)
{
    char dir[] = "/tmp/tcmg-webif-many-XXXXXX";
    static const int counts[] = { 0, 1, 8, 9, 31, 32, 33, 40, 200 };

    if (!mkdtemp(dir)) return 2;
    if (!init_cfg(dir)) return 3;
    snprintf(g_fake_acct.user, sizeof(g_fake_acct.user), "admin");

    for (size_t k = 0; k < sizeof(counts) / sizeof(counts[0]); k++) {
        int n = counts[k];
        set_clients(n);

        char *r = run_handler(h_status);
        if (!r || !strstr(r, "200 OK")) return 10;
        if (count_str(r, "\"thread_id\":") != n) {
            fprintf(stderr, "api/status clients=%d got=%d\n", n, count_str(r, "\"thread_id\":"));
            return 11;
        }
        free(r);

        r = run_handler(h_userstats);
        if (!r || !strstr(r, "200 OK") || !strstr(r, "\"users\":[")) return 12;
        if (n > 0 && !strstr(r, "\"active\":")) return 13;
        free(r);

        r = run_handler(h_users);
        if (!r || !strstr(r, "200 OK") || !strstr(r, "id='usrBody'") || !strstr(r, "</html>")) return 14;
        free(r);
    }

    set_clients(0);
    for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++) free(g_fake[i]);
    printf("WEBIF_MANY_CLIENTS: PASS\n");
    return 0;
}
