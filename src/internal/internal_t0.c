#define MODULE_LOG_PREFIX "internal"
#include "internal_t0.h"
#include "../core/utils.h"
#include "../log/log.h"

#ifndef TCMG_OS_WINDOWS

#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

static int wait_fd(int fd, int writable, uint32_t timeout_ms)
{
    if (fd < 0) return -1;

    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);

    struct timeval tv;
    tv.tv_sec = (time_t)(timeout_ms / 1000u);
    tv.tv_usec = (suseconds_t)((timeout_ms % 1000u) * 1000u);

    int rc;
    do {
        rc = select(fd + 1,
                    writable ? NULL : &set,
                    writable ? &set : NULL,
                    NULL, &tv);
    } while (rc < 0 && errno == EINTR);

    return rc > 0 ? 0 : -1;
}

static int write_all(int fd, const uint8_t *buf, size_t len, uint32_t timeout_ms)
{
    size_t off = 0;
    while (off < len) {
        if (wait_fd(fd, 1, timeout_ms) < 0) return -1;

        ssize_t n = write(fd, buf + off, len - off);
        if (n > 0) {
            off += (size_t)n;
            continue;
        }
        if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
            continue;
        return -1;
    }
    return 0;
}

static int read_exact(int fd, uint8_t *buf, size_t len, uint32_t timeout_ms)
{
    size_t off = 0;
    while (off < len) {
        if (wait_fd(fd, 0, timeout_ms) < 0) return -1;

        ssize_t n = read(fd, buf + off, len - off);
        if (n > 0) {
            off += (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            continue;
        return -1;
    }
    return 0;
}

static int drain(int fd)
{
    if (tcdrain(fd) == 0) return 0;
    if (errno == ENOTTY || errno == EINVAL || errno == ENOSYS)
        return 0;
    return -1;
}

static int is_sw1(uint8_t b)
{
    return (b & 0xF0u) == 0x60u || (b & 0xF0u) == 0x90u;
}

static int is_ack(uint8_t b, uint8_t ins)
{
    return (b & 0x0Eu) == (ins & 0x0Eu);
}

static int is_not_ack(uint8_t b, uint8_t ins)
{
    return (b & 0x0Eu) == ((uint8_t)(~ins) & 0x0Eu);
}

int internal_t0_exchange(const S_INTERNAL_T0_CHANNEL *ch,
                         const uint8_t *apdu, size_t apdu_len,
                         uint8_t *rsp, size_t *rsp_len)
{
    if (!ch || ch->fd < 0 || !apdu || !rsp || !rsp_len)
        return -1;
    if (apdu_len < 5 || apdu_len > 260 || *rsp_len < 2)
        return -2;

    const size_t body_len = apdu_len - 4u;
    const int case2 = body_len == 1u;
    const int case3 = apdu[4] != 0 && body_len == (size_t)apdu[4] + 1u;
    if (!case2 && !case3)
        return -3;

    const uint8_t ins = apdu[1];
    const size_t le = apdu[4] ? apdu[4] : 256u;
    const size_t lc = apdu[4];
    const uint32_t wait_ms = ch->wwt_ms ? ch->wwt_ms : 1500u;
    const uint32_t write_ms = ch->io_write_timeout_ms ? ch->io_write_timeout_ms : 1500u;
    const uint32_t max_nulls = ch->max_nulls ? ch->max_nulls : 32u;

    size_t out = 0;
    size_t sent = 0;
    unsigned nulls = 0;

    tcmg_dump_dbg(D_ECM, apdu, (int32_t)apdu_len, "INTERNAL T0 >> APDU");

    if (write_all(ch->fd, apdu, 5, write_ms) < 0)
        return -10;
    if (drain(ch->fd) < 0)
        return -11;

    for (unsigned guard = 0; guard < 2048; guard++) {
        uint8_t procedure = 0;
        if (read_exact(ch->fd, &procedure, 1, wait_ms) < 0) {
            tcmg_log_dbg(D_ECM, "internal T0 procedure timeout ins=%02X wait=%ums",
                         ins, wait_ms);
            return -12;
        }

        tcmg_log_dbg(D_ECM, "internal T0 procedure ins=%02X byte=%02X", ins, procedure);

        if (procedure == 0x60) {
            if (++nulls >= max_nulls) {
                tcmg_log_dbg(D_ECM, "internal T0 too many NULL bytes ins=%02X count=%u",
                             ins, nulls);
                return -13;
            }
            continue;
        }

        if (is_sw1(procedure)) {
            if (out + 2 > *rsp_len)
                return -14;
            rsp[out++] = procedure;
            if (read_exact(ch->fd, &rsp[out], 1, wait_ms) < 0)
                return -15;
            out++;
            *rsp_len = out;
            tcmg_dump_dbg(D_ECM, rsp, (int32_t)*rsp_len, "INTERNAL T0 << SW");
            return 0;
        }

        if (is_ack(procedure, ins)) {
            nulls = 0;
            if (case3) {
                if (sent != 0 || lc == 0)
                    return -16;
                if (write_all(ch->fd, apdu + 5, lc, write_ms) < 0)
                    return -17;
                if (drain(ch->fd) < 0)
                    return -18;
                sent = lc;
                continue;
            }

            if (out + le + 2 > *rsp_len)
                return -19;
            if (read_exact(ch->fd, rsp + out, le, wait_ms) < 0)
                return -20;
            out += le;
            continue;
        }

        if (is_not_ack(procedure, ins)) {
            nulls = 0;
            if (case3) {
                if (sent >= lc)
                    return -21;
                if (write_all(ch->fd, apdu + 5 + sent, 1, write_ms) < 0)
                    return -22;
                if (drain(ch->fd) < 0)
                    return -23;
                sent++;
                continue;
            }

            if (out >= le)
                return -24;
            if (out + 1 > *rsp_len)
                return -25;
            if (read_exact(ch->fd, &rsp[out], 1, wait_ms) < 0)
                return -26;
            out++;
            continue;
        }

        tcmg_log_dbg(D_ECM, "internal T0 unexpected procedure ins=%02X byte=%02X",
                     ins, procedure);
        return -27;
    }

    return -28;
}

#else

int internal_t0_exchange(const S_INTERNAL_T0_CHANNEL *ch,
                         const uint8_t *apdu, size_t apdu_len,
                         uint8_t *rsp, size_t *rsp_len)
{
    (void)ch;
    (void)apdu;
    (void)apdu_len;
    (void)rsp;
    (void)rsp_len;
    return -100;
}

#endif
