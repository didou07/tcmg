#include "internal/internal_t0.h"
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

_Atomic uint16_t g_dblevel = 0;

void tcmg_log_txt(const char *mod, const char *fmt, ...)
{
    (void)mod; (void)fmt;
}
void tcmg_log_force_txt(const char *mod, const char *fmt, ...)
{
    (void)mod; (void)fmt;
}
void tcmg_log_hex(const char *mod, const uint8_t *buf, int32_t n, const char *fmt, ...)
{
    (void)mod; (void)buf; (void)n; (void)fmt;
}

struct card_ctx {
    int fd;
    int mode;
    int rc;
};

static int read_full(int fd, void *buf, size_t len)
{
    size_t off = 0;
    while (off < len) {
        ssize_t n = read(fd, (uint8_t *)buf + off, len - off);
        if (n > 0) { off += (size_t)n; continue; }
        if (n < 0 && errno == EINTR) continue;
        return -1;
    }
    return 0;
}

static void *card_thread(void *arg)
{
    struct card_ctx *c = (struct card_ctx *)arg;
    uint8_t hdr[5];
    if (read_full(c->fd, hdr, sizeof(hdr)) < 0) { c->rc = 1; return NULL; }

    if (c->mode == 1) {
        if (memcmp(hdr, (uint8_t[]){0xDD,0xA2,0x00,0x00,0x06}, 5) != 0) { c->rc = 2; return NULL; }
        if (write(c->fd, "\x60\xA2", 2) != 2) { c->rc = 3; return NULL; }
        uint8_t data[6];
        if (read_full(c->fd, data, sizeof(data)) < 0) { c->rc = 4; return NULL; }
        if (memcmp(data, (uint8_t[]){0x14,0x04,0x00,0x80,0x01,0x02}, 6) != 0) { c->rc = 5; return NULL; }
        if (write(c->fd, "\x90\x00", 2) != 2) { c->rc = 6; return NULL; }
    } else if (c->mode == 2) {
        if (memcmp(hdr, (uint8_t[]){0xDD,0xA2,0x00,0x00,0x06}, 5) != 0) { c->rc = 7; return NULL; }
        const uint8_t one_ack = 0x5D;
        const uint8_t expected[6] = {0x14,0x04,0x00,0x80,0x01,0x02};
        for (size_t i = 0; i < sizeof(expected); i++) {
            if (write(c->fd, &one_ack, 1) != 1) { c->rc = 8; return NULL; }
            uint8_t b = 0;
            if (read_full(c->fd, &b, 1) < 0 || b != expected[i]) { c->rc = 9 + (int)i; return NULL; }
        }
        if (write(c->fd, "\x90\x00", 2) != 2) { c->rc = 16; return NULL; }
    } else if (c->mode == 3) {
        if (memcmp(hdr, (uint8_t[]){0xDD,0xCA,0x00,0x00,0x03}, 5) != 0) { c->rc = 12; return NULL; }
        if (write(c->fd, "\x60\xCA", 2) != 2) { c->rc = 13; return NULL; }
        if (write(c->fd, "\x11\x22\x33\x90\x00", 5) != 5) { c->rc = 14; return NULL; }
    }
    c->rc = 0;
    return NULL;
}

static int run_case(int mode)
{
    int sp[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp) != 0) return 100 + mode;

    struct card_ctx card = { .fd = sp[1], .mode = mode, .rc = -1 };
    pthread_t tid;
    if (pthread_create(&tid, NULL, card_thread, &card) != 0) return 110 + mode;

    S_INTERNAL_T0_CHANNEL ch = {
        .fd = sp[0],
        .wwt_ms = 1000,
        .io_write_timeout_ms = 1000,
        .max_nulls = 8
    };
    uint8_t apdu[16];
    size_t apdu_len;
    uint8_t rsp[32];
    size_t rsp_len = sizeof(rsp);
    int rc;

    if (mode == 3) {
        memcpy(apdu, (uint8_t[]){0xDD,0xCA,0x00,0x00,0x03}, 5);
        apdu_len = 5;
        rc = internal_t0_exchange(&ch, apdu, apdu_len, rsp, &rsp_len);
        if (rc != 0 || rsp_len != 5 || memcmp(rsp, (uint8_t[]){0x11,0x22,0x33,0x90,0x00}, 5) != 0)
            card.rc = 20;
    } else {
        memcpy(apdu, (uint8_t[]){0xDD,0xA2,0x00,0x00,0x06,0x14,0x04,0x00,0x80,0x01,0x02}, 11);
        apdu_len = 11;
        rc = internal_t0_exchange(&ch, apdu, apdu_len, rsp, &rsp_len);
        if (rc != 0 || rsp_len != 2 || memcmp(rsp, (uint8_t[]){0x90,0x00}, 2) != 0)
            card.rc = 20;
    }

    shutdown(sp[0], SHUT_RDWR);
    shutdown(sp[1], SHUT_RDWR);
    close(sp[0]);
    close(sp[1]);
    pthread_join(tid, NULL);
    return card.rc == 0 ? 0 : card.rc;
}

int main(void)
{
    for (int mode = 1; mode <= 3; mode++) {
        int rc = run_case(mode);
        if (rc != 0) {
            fprintf(stderr, "INTERNAL_T0: FAIL mode=%d rc=%d\n", mode, rc);
            return 1;
        }
    }
    puts("INTERNAL_T0: PASS");
    return 0;
}
