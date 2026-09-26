#define MODULE_LOG_PREFIX "serial"
#include "serial.h"
#include "../config/runtime_access.h"
#include "../core/constants.h"
#include "../core/utils.h"
#include "../platform/platform.h"
#include "../log/log.h"

#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef TCMG_OS_WINDOWS
#include <strings.h>
#endif

#ifndef TCMG_OS_WINDOWS
#include <fcntl.h>
#include <glob.h>
#include <sys/select.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/ioctl.h>
#endif
#else
#include <windows.h>
#include <winreg.h>
#endif

#define SERIAL_BAUD 9600
#define SERIAL_OPEN_SETTLE_MS 50
#define SERIAL_IO_TIMEOUT_MS 1500
#define SERIAL_ATR_TIMEOUT_MS 1000
#define SERIAL_T0_GAP_MS 10

typedef struct {
#ifdef TCMG_OS_WINDOWS
    HANDLE h;
#else
    int fd;
#endif
    int open;
    int present;
    int ready;
    int protocol;
    int parity;
    int64_t last_reset_ms;
    int64_t last_attempt_ms;
    int64_t last_poll_ms;
    uint8_t atr[TCMG_SERIAL_MAX_ATR];
    size_t atr_len;
    char device[TCMG_SERIAL_PORT_LEN];
    pthread_mutex_t mtx;
} S_SERIAL_SLOT;

static S_SERIAL_SLOT s_slots[MAX_READERS];
static pthread_t s_tid;
static _Atomic int8_t s_running = 0;
static pthread_mutex_t s_init_mtx = PTHREAD_MUTEX_INITIALIZER;
static int s_initialized = 0;

static int64_t mono_ms(void)
{
    return tcmg_mono_ms();
}

static void slots_init(void)
{
    pthread_mutex_lock(&s_init_mtx);
    if (!s_initialized) {
        for (int i = 0; i < MAX_READERS; i++) {
#ifdef TCMG_OS_WINDOWS
            s_slots[i].h = INVALID_HANDLE_VALUE;
#else
            s_slots[i].fd = -1;
#endif
            pthread_mutex_init(&s_slots[i].mtx, NULL);
        }
        s_initialized = 1;
    }
    pthread_mutex_unlock(&s_init_mtx);
}

static void slot_close(S_SERIAL_SLOT *s)
{
    if (!s) return;
#ifdef TCMG_OS_WINDOWS
    if (s->h != INVALID_HANDLE_VALUE) {
        CloseHandle(s->h);
        s->h = INVALID_HANDLE_VALUE;
    }
#else
    if (s->fd >= 0) {
        close(s->fd);
        s->fd = -1;
    }
#endif
    s->open = 0;
}

static void slot_clear(S_SERIAL_SLOT *s)
{
    if (!s) return;
    slot_close(s);
    s->present = 0;
    s->ready = 0;
    s->protocol = 0;
    s->parity = -1;
    s->last_reset_ms = 0;
    s->last_attempt_ms = 0;
    s->last_poll_ms = 0;
    s->atr_len = 0;
    memset(s->atr, 0, sizeof(s->atr));
    s->device[0] = '\0';
}

#ifdef TCMG_OS_WINDOWS
static void normalize_com(const char *in, wchar_t *out, size_t cap)
{
    char path[TCMG_SERIAL_PORT_LEN];
    const char *p = in ? in : "";
    tcmg_strlcpy(path, p, sizeof(path));
    while (isspace((unsigned char)*path)) memmove(path, path + 1, strlen(path));
    if (_strnicmp(path, "\\\\.\\", 4) == 0) {
        MultiByteToWideChar(CP_UTF8, 0, path, -1, out, (int)cap);
        return;
    }
    if (_strnicmp(path, "COM", 3) == 0) {
        char buf[TCMG_SERIAL_PORT_LEN];
        snprintf(buf, sizeof(buf), "\\\\.\\%s", path);
        MultiByteToWideChar(CP_UTF8, 0, buf, -1, out, (int)cap);
        return;
    }
    char buf[TCMG_SERIAL_PORT_LEN];
    snprintf(buf, sizeof(buf), "\\\\.\\COM%s", path);
    MultiByteToWideChar(CP_UTF8, 0, buf, -1, out, (int)cap);
}

static int win_set_rts(HANDLE h, int high)
{
    return EscapeCommFunction(h, high ? SETRTS : CLRRTS) ? 0 : -1;
}

static int win_configure(HANDLE h, int parity)
{
    DCB dcb;
    COMMTIMEOUTS to;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) return -1;
    dcb.BaudRate = SERIAL_BAUD;
    dcb.ByteSize = 8;
    dcb.StopBits = TWOSTOPBITS;
    dcb.Parity = (parity == 0) ? EVENPARITY : (parity == 1 ? ODDPARITY : NOPARITY);
    dcb.fBinary = TRUE;
    dcb.fParity = (parity != 2);
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fInX = FALSE;
    dcb.fOutX = FALSE;
    if (!SetCommState(h, &dcb)) return -1;
    memset(&to, 0, sizeof(to));
    to.ReadIntervalTimeout = 50;
    to.ReadTotalTimeoutMultiplier = 0;
    to.ReadTotalTimeoutConstant = SERIAL_IO_TIMEOUT_MS;
    to.WriteTotalTimeoutMultiplier = 0;
    to.WriteTotalTimeoutConstant = 1500;
    if (!SetCommTimeouts(h, &to)) return -1;
    PurgeComm(h, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
    return 0;
}

static int serial_open_slot(S_SERIAL_SLOT *s, const char *device, int parity)
{
    wchar_t wide[TCMG_SERIAL_PORT_LEN];
    normalize_com(device, wide, sizeof(wide) / sizeof(wide[0]));
    HANDLE h = CreateFileW(wide, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return -1;
    if (win_configure(h, parity) < 0) {
        CloseHandle(h);
        return -1;
    }
    s->h = h;
    s->open = 1;
    s->parity = parity;
    return 0;
}

static int serial_write_all(S_SERIAL_SLOT *s, const uint8_t *buf, size_t len, int timeout_ms)
{
    (void)timeout_ms;
    while (len) {
        DWORD n = 0;
        if (!WriteFile(s->h, buf, (DWORD)len, &n, NULL)) return -1;
        if (!n) return -1;
        buf += n;
        len -= n;
    }
    return 0;
}

static int serial_read_byte(S_SERIAL_SLOT *s, uint8_t *out, int timeout_ms)
{
    COMMTIMEOUTS to;
    memset(&to, 0, sizeof(to));
    to.ReadIntervalTimeout = 50;
    to.ReadTotalTimeoutMultiplier = 0;
    to.ReadTotalTimeoutConstant = (DWORD)timeout_ms;
    SetCommTimeouts(s->h, &to);
    DWORD n = 0;
    if (!ReadFile(s->h, out, 1, &n, NULL)) return -1;
    return n == 1 ? 0 : -1;
}
#else
static int unix_configure(int fd, int parity)
{
    struct termios tio;
    if (tcgetattr(fd, &tio) < 0) return -1;
    cfmakeraw(&tio);
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag &= ~(CSIZE | CRTSCTS | CSTOPB);
    tio.c_cflag |= CS8 | CSTOPB;
    if (parity == 2) {
        tio.c_cflag &= ~PARENB;
        tio.c_cflag &= ~PARODD;
    } else {
        tio.c_cflag |= PARENB;
        if (parity == 0) tio.c_cflag &= ~PARODD;
        else tio.c_cflag |= PARODD;
    }
    cfsetispeed(&tio, B9600);
    cfsetospeed(&tio, B9600);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 1;
    if (tcsetattr(fd, TCSANOW, &tio) < 0) return -1;
    tcflush(fd, TCIOFLUSH);
    return 0;
}

static int unix_set_rts(int fd, int high)
{
#if defined(TIOCM_RTS) && (defined(TIOCMBIS) || defined(TIOCMBIC))
    int bits = TIOCM_RTS;
    return ioctl(fd, high ? TIOCMBIS : TIOCMBIC, &bits) == 0 ? 0 : -1;
#elif defined(TIOCM_RTS) && defined(TIOCMGET) && defined(TIOCMSET)
    int bits = 0;
    if (ioctl(fd, TIOCMGET, &bits) < 0) return -1;
    if (high) bits |= TIOCM_RTS;
    else bits &= ~TIOCM_RTS;
    return ioctl(fd, TIOCMSET, &bits) == 0 ? 0 : -1;
#else
    (void)fd; (void)high;
    return -1;
#endif
}

static int serial_open_slot(S_SERIAL_SLOT *s, const char *device, int parity)
{
    int fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return -1;
    if (unix_configure(fd, parity) < 0) {
        close(fd);
        return -1;
    }
    s->fd = fd;
    s->open = 1;
    s->parity = parity;
    return 0;
}

static int serial_write_all(S_SERIAL_SLOT *s, const uint8_t *buf, size_t len, int timeout_ms)
{
    size_t off = 0;
    while (off < len) {
        fd_set wf;
        FD_ZERO(&wf); FD_SET(s->fd, &wf);
        struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
        int rc = select(s->fd + 1, NULL, &wf, NULL, &tv);
        if (rc <= 0) return -1;
        ssize_t n = write(s->fd, buf + off, len - off);
        if (n > 0) off += (size_t)n;
        else if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        else return -1;
    }
    return 0;
}

static int serial_read_byte(S_SERIAL_SLOT *s, uint8_t *out, int timeout_ms)
{
    fd_set rf;
    FD_ZERO(&rf); FD_SET(s->fd, &rf);
    struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
    int rc = select(s->fd + 1, &rf, NULL, NULL, &tv);
    if (rc <= 0) return -1;
    for (;;) {
        ssize_t n = read(s->fd, out, 1);
        if (n == 1) return 0;
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return -1;
        return -1;
    }
}
#endif

static int serial_read_n(S_SERIAL_SLOT *s, uint8_t *buf, size_t len, int timeout_ms)
{
    for (size_t i = 0; i < len; i++)
        if (serial_read_byte(s, &buf[i], timeout_ms) < 0) return -1;
    return 0;
}

static int serial_drain(S_SERIAL_SLOT *s, size_t len)
{
    uint8_t b;
    for (size_t i = 0; i < len; i++)
        if (serial_read_byte(s, &b, 2500) < 0) return -1;
    return 0;
}

static int read_atr(S_SERIAL_SLOT *s)
{
    uint8_t *a = s->atr;
    memset(a, 0, sizeof(s->atr));
    s->atr_len = 0;

    if (serial_read_byte(s, &a[0], 3000) < 0) return -1;
    if (a[0] != 0x3B && a[0] != 0x3F) return -1;
    if (serial_read_byte(s, &a[1], 500) < 0) return -1;

    uint8_t y = (uint8_t)((a[1] >> 4) & 0x0F);
    int hist = a[1] & 0x0F;
    int group = 1;
    int has_tck = 0;
    int proto = 0;
    size_t n = 2;

    for (;;) {
        if (y & 0x01) { if (n >= sizeof(s->atr) || serial_read_byte(s, &a[n++], 500) < 0) return -1; }
        if (y & 0x02) { if (n >= sizeof(s->atr) || serial_read_byte(s, &a[n++], 500) < 0) return -1; }
        if (y & 0x04) { if (n >= sizeof(s->atr) || serial_read_byte(s, &a[n++], 500) < 0) return -1; }
        if (y & 0x08) {
            if (n >= sizeof(s->atr) || serial_read_byte(s, &a[n++], 500) < 0) return -1;
            y = (uint8_t)((a[n - 1] >> 4) & 0x0F);
            proto = a[n - 1] & 0x0F;
            if (proto != 0) has_tck = 1;
            group++;
            if (group > 8) return -1;
            continue;
        }
        break;
    }

    if (n + (size_t)hist + (has_tck ? 1u : 0u) > sizeof(s->atr)) return -1;
    for (int i = 0; i < hist; i++)
        if (serial_read_byte(s, &a[n++], 500) < 0) return -1;
    if (has_tck && serial_read_byte(s, &a[n++], 500) < 0) return -1;

    if (a[0] == 0x3F) return -1;
    s->atr_len = n;
    s->protocol = 0;
    return 0;
}

static int do_reset_parity(S_SERIAL_SLOT *s, int parity, int *got)
{
    slot_close(s);
#ifdef TCMG_OS_WINDOWS
    if (serial_open_slot(s, s->device, parity) < 0) return -1;
    if (win_set_rts(s->h, 1) < 0) { slot_close(s); return -1; }
#else
    if (serial_open_slot(s, s->device, parity) < 0) return -1;
    if (unix_set_rts(s->fd, 1) < 0) { slot_close(s); return -1; }
#endif
    tcmg_sleep_ms(SERIAL_OPEN_SETTLE_MS);
    slot_close(s);

    if (serial_open_slot(s, s->device, parity) < 0) return -1;
#ifdef TCMG_OS_WINDOWS
    if (win_set_rts(s->h, 0) < 0) { slot_close(s); return -1; }
#else
    if (unix_set_rts(s->fd, 0) < 0) { slot_close(s); return -1; }
#endif
    tcmg_sleep_ms(SERIAL_OPEN_SETTLE_MS);
    if (read_atr(s) < 0) {
        slot_close(s);
        return -1;
    }
    s->parity = parity;
    s->present = 1;
    s->ready = 1;
    if (got) *got = 1;
    return 0;
}

static int serial_fast_reset(S_SERIAL_SLOT *s)
{
    static const int parities[] = { 0, 1, 2 };
    int got = 0;
    for (size_t i = 0; i < sizeof(parities) / sizeof(parities[0]); i++) {
        if (do_reset_parity(s, parities[i], &got) == 0) {
            s->last_reset_ms = mono_ms();
            tcmg_log_force("fast reset ok device=%s parity=%s ATR=%zu",
                           s->device, parities[i] == 0 ? "even" : (parities[i] == 1 ? "odd" : "none"), s->atr_len);
            return 0;
        }
    }
    s->present = 0;
    s->ready = 0;
    s->atr_len = 0;
    if (!got) tcmg_log_dbg(D_READER, "fast reset failed device=%s", s->device);
    return -1;
}

static int t0_transmit(S_SERIAL_SLOT *s, const uint8_t *apdu, size_t apdu_len,
                       uint8_t *rsp, size_t *rsp_len)
{
    if (!s || !s->open || !apdu || apdu_len < 5 || apdu_len > 260 || !rsp || !rsp_len || *rsp_len < 2)
        return -1;

    size_t body = apdu_len - 4;
    int case2 = (body == 1);
    int case3 = (apdu[4] > 0 && body == (size_t)apdu[4] + 1u);
    if (!case2 && !case3) return -2;

    if (serial_write_all(s, apdu, 5, 1500) < 0) return -3;
    if (serial_drain(s, 5) < 0) return -4;

    const uint8_t ins = apdu[1];
    const size_t le = apdu[4] ? apdu[4] : 256u;
    const size_t lc = apdu[4];
    size_t out = 0;
    size_t sent = 0;

    for (int guard = 0; guard < 600; guard++) {
        uint8_t p = 0;
        if (serial_read_byte(s, &p, 2500) < 0) return -5;
        if (p == 0x60) continue;
        if ((p & 0xF0) == 0x60 || (p & 0xF0) == 0x90) {
            if (out + 2 > *rsp_len) return -6;
            rsp[out++] = p;
            if (serial_read_byte(s, &rsp[out++], 2500) < 0) return -7;
            *rsp_len = out;
            return 0;
        }

        if ((p & 0x0E) == (ins & 0x0E)) {
            if (!case3) {
                if (le + 2 > *rsp_len) return -8;
                if (serial_read_n(s, rsp, le + 2, 2500) < 0) return -9;
                *rsp_len = le + 2;
                return 0;
            }
            if (sent != 0) return -10;
            if (serial_write_all(s, apdu + 5, lc, 1500) < 0) return -11;
            if (serial_drain(s, lc) < 0) return -12;
            sent = lc;
        }
        else if ((p & 0x0E) == ((uint8_t)(~ins) & 0x0E)) {
            if (!case3) {
                if (out + 1 > *rsp_len) return -13;
                if (serial_read_byte(s, &rsp[out++], 2500) < 0) return -14;
                if (out >= le) {
                    if (out + 2 > *rsp_len) return -15;
                    if (serial_read_n(s, rsp + out, 2, 2500) < 0) return -16;
                    out += 2;
                    *rsp_len = out;
                    return 0;
                }
            } else {
                if (sent >= lc) return -17;
                if (serial_write_all(s, apdu + 5 + sent, 1, 1500) < 0) return -18;
                if (serial_drain(s, 1) < 0) return -19;
                sent++;
            }
        }
        else return -20;
    }
    return -21;
}

static int parse_conax_cw(const uint8_t *rsp, size_t rsp_len, uint8_t cw[16], int *found_mask)
{
    if (!rsp || rsp_len < 2 || !cw || !found_mask) return -1;
    *found_mask = 0;
    if (rsp_len >= 3 && rsp[0] == 0x81 && ((rsp[2] >> 5) == 2)) return -2;
    size_t data_len = rsp_len - 2;
    size_t p = 0;
    while (p + 2 <= data_len) {
        uint8_t tag = rsp[p];
        uint8_t len = rsp[p + 1];
        size_t end = p + 2u + len;
        if (end > data_len) break;
        if (tag == 0x25 && len >= 0x0D) {
            uint8_t n = rsp[p + 4];
            if (n < 2 && p + 15 <= data_len) {
                memcpy(cw + ((size_t)n << 3), rsp + p + 7, 8);
                *found_mask |= (1 << n);
            }
        }
        p = end;
    }
    return 0;
}

static int conax_ecm(S_SERIAL_SLOT *s, const uint8_t *ecm, size_t ecm_len, uint8_t cw[16])
{
    uint8_t apdu[260];
    uint8_t rsp[320];
    size_t rsp_len = sizeof(rsp);
    if (!ecm || ecm_len == 0 || ecm_len > 249 || !cw) return -1;

    size_t apdu_len = 8 + ecm_len;
    apdu[0] = 0xDD;
    apdu[1] = 0xA2;
    apdu[2] = 0x00;
    apdu[3] = 0x00;
    apdu[4] = (uint8_t)(ecm_len + 3);
    apdu[5] = 0x14;
    apdu[6] = (uint8_t)(ecm_len + 1);
    apdu[7] = 0x00;
    memcpy(apdu + 8, ecm, ecm_len);

    if (t0_transmit(s, apdu, apdu_len, rsp, &rsp_len) < 0) return -2;
    if (rsp_len < 2) return -3;

    uint8_t sw1 = rsp[rsp_len - 2];
    uint8_t sw2 = rsp[rsp_len - 1];
    if (sw1 == 0x90 && sw2 == 0x11) return -4;
    if (sw1 != 0x90 && sw1 != 0x98) return -5;

    int got = 0;
    if (sw1 == 0x90 && sw2 == 0x00) {
        if (parse_conax_cw(rsp, rsp_len, cw, &got) == -2) return -6;
    }

    while (sw1 == 0x98 && sw2 != 0x00 && sw2 != 0xFF) {
        uint8_t ins_ca[5] = {0xDD, 0xCA, 0x00, 0x00, sw2};
        rsp_len = sizeof(rsp);
        if (t0_transmit(s, ins_ca, sizeof(ins_ca), rsp, &rsp_len) < 0) return -7;
        if (rsp_len < 2) return -8;
        sw1 = rsp[rsp_len - 2];
        sw2 = rsp[rsp_len - 1];
        if (sw1 == 0x98 || (sw1 == 0x90 && sw2 == 0x00)) {
            int part = 0;
            if (parse_conax_cw(rsp, rsp_len, cw, &part) == -2) return -9;
            got |= part;
        } else return -10;
    }
    return got == 3 ? 0 : -11;
}

static int serial_open_and_reset(S_SERIAL_SLOT *s, const char *device)
{
    if (!device || !*device) return -1;
    if (s->device[0] && strcmp(s->device, device) != 0) slot_clear(s);
    if (!s->device[0]) tcmg_strlcpy(s->device, device, sizeof(s->device));
    if (!s->ready) return serial_fast_reset(s);
    return 0;
}

static void *serial_thread(void *arg)
{
    (void)arg;
    while (atomic_load(&s_running)) {
        int min_poll = 1000;
        S_READER cfg_readers[MAX_READERS];
        int reader_count = cfg_runtime_reader_snapshot_indexed(cfg_readers, MAX_READERS);

        for (int i = 0; i < MAX_READERS; i++) {
            S_READER *cfg = (i < reader_count) ? &cfg_readers[i] : NULL;
            int want = cfg && cfg->in_use && cfg->enabled && !strcasecmp(cfg->protocol, "serial");
            const char *device = want ? cfg->device : NULL;
            if (want && cfg->poll_ms > 0 && cfg->poll_ms < min_poll) min_poll = cfg->poll_ms;

            pthread_mutex_lock(&s_slots[i].mtx);
            if (!want || !device || !*device) {
                slot_clear(&s_slots[i]);
            } else {
                if (s_slots[i].device[0] && strcmp(s_slots[i].device, device) != 0) slot_clear(&s_slots[i]);
                tcmg_strlcpy(s_slots[i].device, device, sizeof(s_slots[i].device));
                int64_t now = mono_ms();
                if (!s_slots[i].ready &&
                    (s_slots[i].last_attempt_ms == 0 || now - s_slots[i].last_attempt_ms >= 2000)) {
                    s_slots[i].last_attempt_ms = now;
                    if (serial_fast_reset(&s_slots[i]) < 0)
                        tcmg_log_dbg(D_READER, "reader[%d]: reset failed device=%s", i + 1, device);
                }
                if (s_slots[i].ready && cfg->fast_reset > 0 &&
                    now - s_slots[i].last_reset_ms >= (int64_t)cfg->fast_reset * 1000LL) {
                    if (serial_fast_reset(&s_slots[i]) < 0)
                        tcmg_log("reader[%d]: fast reset failed device=%s", i + 1, device);
                }
                s_slots[i].last_poll_ms = now;
            }
            pthread_mutex_unlock(&s_slots[i].mtx);
        }

        if (min_poll < 50) min_poll = 50;
        if (min_poll > 10000) min_poll = 10000;
        tcmg_sleep_ms(min_poll);
    }
    return NULL;
}

int serial_start(void)
{
    slots_init();
    if (atomic_exchange(&s_running, 1)) return 0;
    if (pthread_create(&s_tid, NULL, serial_thread, NULL) != 0) {
        atomic_store(&s_running, 0);
        return -1;
    }
    tcmg_log("reader support enabled");
    return 0;
}

void serial_stop(void)
{
    slots_init();
    if (atomic_exchange(&s_running, 0)) pthread_join(s_tid, NULL);
    for (int i = 0; i < MAX_READERS; i++) {
        pthread_mutex_lock(&s_slots[i].mtx);
        slot_clear(&s_slots[i]);
        pthread_mutex_unlock(&s_slots[i].mtx);
    }
}

int serial_reader_get(int index, S_SERIAL_READER *out)
{
    if (!out || index < 0 || index >= MAX_READERS) return -1;
    slots_init();
    pthread_mutex_lock(&s_slots[index].mtx);
    memset(out, 0, sizeof(*out));
    tcmg_strlcpy(out->device, s_slots[index].device, sizeof(out->device));
    out->present = s_slots[index].present;
    out->ready = s_slots[index].ready;
    out->atr_len = s_slots[index].atr_len;
    if (out->atr_len > sizeof(out->atr)) out->atr_len = sizeof(out->atr);
    memcpy(out->atr, s_slots[index].atr, out->atr_len);
    out->protocol = s_slots[index].protocol;
    pthread_mutex_unlock(&s_slots[index].mtx);
    return out->device[0] ? 0 : -1;
}

int serial_reader_count(void)
{
    int n = 0;
    slots_init();
    for (int i = 0; i < MAX_READERS; i++) {
        pthread_mutex_lock(&s_slots[i].mtx);
        if (s_slots[i].device[0]) n++;
        pthread_mutex_unlock(&s_slots[i].mtx);
    }
    return n;
}

int serial_do_ecm_reader(int index, uint16_t caid,
                         const uint8_t *ecm, size_t ecm_len,
                         uint8_t cw[16], int32_t whitelist)
{
    if (index < 0 || index >= MAX_READERS || !ecm || !cw || ecm_len == 0 || ecm_len > 249) return -1;
    if ((caid & 0xFF00u) != 0x0B00u) return -2;
    if (whitelist > 0 && ecm_len > (size_t)whitelist) return -3;

    slots_init();
    S_READER cfg;
    if (!cfg_runtime_reader_get(index, &cfg)) return -4;
    if (!cfg.enabled || strcasecmp(cfg.protocol, "serial") != 0) return -5;

    S_SERIAL_SLOT *s = &s_slots[index];
    pthread_mutex_lock(&s->mtx);
    if (serial_open_and_reset(s, cfg.device) < 0) {
        pthread_mutex_unlock(&s->mtx);
        return -6;
    }

    int rc = conax_ecm(s, ecm, ecm_len, cw);
    if (rc < 0) {
        (void)serial_fast_reset(s);
        rc = conax_ecm(s, ecm, ecm_len, cw);
    }
    pthread_mutex_unlock(&s->mtx);
    return rc;
}

size_t serial_list_ports(char out[][TCMG_SERIAL_PORT_LEN], size_t max_ports)
{
    if (!out || max_ports == 0) return 0;
#ifdef TCMG_OS_WINDOWS
    HKEY key = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &key) != ERROR_SUCCESS)
        return 0;
    size_t count = 0;
    for (DWORD i = 0; count < max_ports; i++) {
        wchar_t name[256]; wchar_t data[256]; DWORD name_len = (DWORD)(sizeof(name) / sizeof(name[0]));
        DWORD data_len = (DWORD)(sizeof(data)); DWORD type = 0;
        LONG rc = RegEnumValueW(key, i, name, &name_len, NULL, &type, (LPBYTE)data, &data_len);
        if (rc == ERROR_NO_MORE_ITEMS) break;
        if (rc != ERROR_SUCCESS || type != REG_SZ) continue;
        char utf8[TCMG_SERIAL_PORT_LEN];
        int n = WideCharToMultiByte(CP_UTF8, 0, data, (int)(data_len / sizeof(wchar_t)), utf8, (int)sizeof(utf8) - 1, NULL, NULL);
        if (n <= 0) continue;
        utf8[n] = '\0';
        if (strncmp(utf8, "COM", 3) != 0) continue;
        tcmg_strlcpy(out[count++], utf8, TCMG_SERIAL_PORT_LEN);
    }
    RegCloseKey(key);
    return count;
#else
    static const char *patterns[] = {
        "/dev/ttyUSB*", "/dev/ttyACM*", "/dev/ttyS*", "/dev/cu.*", "/dev/tty.usb*"
    };
    size_t count = 0;
    for (size_t p = 0; p < sizeof(patterns) / sizeof(patterns[0]) && count < max_ports; p++) {
        glob_t g;
        memset(&g, 0, sizeof(g));
        if (glob(patterns[p], 0, NULL, &g) == 0) {
            for (size_t i = 0; i < g.gl_pathc && count < max_ports; i++) {
                int dup = 0;
                for (size_t j = 0; j < count; j++) if (!strcmp(out[j], g.gl_pathv[i])) { dup = 1; break; }
                if (!dup) tcmg_strlcpy(out[count++], g.gl_pathv[i], TCMG_SERIAL_PORT_LEN);
            }
        }
        globfree(&g);
    }
    for (size_t i = 0; i < count; i++) {
        for (size_t j = i + 1; j < count; j++) {
            if (strcmp(out[j], out[i]) < 0) {
                char tmp[TCMG_SERIAL_PORT_LEN];
                memcpy(tmp, out[i], sizeof(tmp));
                memcpy(out[i], out[j], sizeof(tmp));
                memcpy(out[j], tmp, sizeof(tmp));
            }
        }
    }
    return count;
#endif
}
