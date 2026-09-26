#define MODULE_LOG_PREFIX "internal"
#include "internal.h"
#include "../config/runtime_access.h"
#include "../core/constants.h"
#include "../core/utils.h"
#include "../log/log.h"
#include "../platform/platform.h"

#define TCMG_T0_RESPONSE_GAP_MS 10
#define TCMG_INTERNAL_ECM_QUEUE_CAP 16
#define TCMG_INTERNAL_ECM_WAIT_MS 20000

enum internal_job_state {
    INTERNAL_JOB_FREE = 0,
    INTERNAL_JOB_QUEUED,
    INTERNAL_JOB_RUNNING,
    INTERNAL_JOB_DONE,
    INTERNAL_JOB_CANCELLED
};

typedef struct {
    int state;
    int rc;
    uint8_t ecm[249];
    size_t ecm_len;
    uint8_t cw[16];
    unsigned waiters;
} S_INTERNAL_JOB;

#ifndef TCMG_OS_WINDOWS

#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>

#ifdef __linux__
#include <stdint.h>
#define SCI_IOW_MAGIC 's'
typedef struct sci_parameters {
    unsigned char T;
    uint32_t fs;
    uint32_t ETU;
    uint32_t WWT;
    uint32_t CWT;
    uint32_t BWT;
    uint32_t EGT;
    uint32_t clock_stop_polarity;
    unsigned char check;
    unsigned char P;
    unsigned char I;
    unsigned char U;
} SCI_PARAMETERS;
#define SCI_SET_RESET _IOW(SCI_IOW_MAGIC, 1, uint32_t)
#define SCI_SET_PARAMETERS _IOW(SCI_IOW_MAGIC, 4, SCI_PARAMETERS)
#define SCI_GET_PARAMETERS _IOW(SCI_IOW_MAGIC, 5, SCI_PARAMETERS)
#define SCI_GET_PRESENT _IOW(SCI_IOW_MAGIC, 8, uint32_t)
#define SCI_SET_ATR_READY _IOW(SCI_IOW_MAGIC, 11, uint32_t)
#endif

typedef struct {
    int fd;
    int exclusive;
    int params_applied;
    int present;
    int ready;
    int protocol;
    int reset_requested;
    int64_t last_reset_ms;
    int worker_running;
    int worker_stop;
    int queue_head;
    int queue_tail;
    int queue_count;
    int queue_slots[TCMG_INTERNAL_ECM_QUEUE_CAP];
    S_INTERNAL_JOB jobs[TCMG_INTERNAL_ECM_QUEUE_CAP];
    uint8_t atr[TCMG_INTERNAL_MAX_ATR];
    size_t atr_len;
    char device[256];
    char target_device[256];
    pthread_t worker_tid;
    pthread_cond_t cv;
    pthread_mutex_t mtx;
} S_INTERNAL_SLOT;

static S_INTERNAL_SLOT s_slots[MAX_READERS];
static pthread_t s_tid;
static _Atomic int8_t s_running = 0;
static pthread_mutex_t s_slots_init_mtx = PTHREAD_MUTEX_INITIALIZER;
static int s_slots_initialized = 0;


static void slots_init(void)
{
    pthread_mutex_lock(&s_slots_init_mtx);
    if (!s_slots_initialized) {
        for (int i = 0; i < MAX_READERS; i++) {
            s_slots[i].fd = -1;
            pthread_mutex_init(&s_slots[i].mtx, NULL);
            pthread_cond_init(&s_slots[i].cv, NULL);
        }
        s_slots_initialized = 1;
    }
    pthread_mutex_unlock(&s_slots_init_mtx);
}

static void queue_reset_locked(S_INTERNAL_SLOT *s)
{
    s->queue_head = 0;
    s->queue_tail = 0;
    s->queue_count = 0;
    for (int i = 0; i < TCMG_INTERNAL_ECM_QUEUE_CAP; i++) {
        s->queue_slots[i] = -1;
        s->jobs[i].state = INTERNAL_JOB_FREE;
        s->jobs[i].rc = -1;
        s->jobs[i].ecm_len = 0;
        memset(s->jobs[i].ecm, 0, sizeof(s->jobs[i].ecm));
        memset(s->jobs[i].cw, 0, sizeof(s->jobs[i].cw));
        s->jobs[i].waiters = 0;
    }
}

static int queue_submit_locked(S_INTERNAL_SLOT *s, const uint8_t *ecm, size_t ecm_len, int *joined)
{
    if (joined) *joined = 0;

    for (int i = 0; i < TCMG_INTERNAL_ECM_QUEUE_CAP; i++) {
        S_INTERNAL_JOB *job = &s->jobs[i];
        if ((job->state == INTERNAL_JOB_QUEUED || job->state == INTERNAL_JOB_RUNNING) &&
            job->ecm_len == ecm_len && memcmp(job->ecm, ecm, ecm_len) == 0) {
            job->waiters++;
            if (joined) *joined = 1;
            return i;
        }
    }

    if (s->queue_count >= TCMG_INTERNAL_ECM_QUEUE_CAP) return -1;

    int job_index = -1;
    for (int i = 0; i < TCMG_INTERNAL_ECM_QUEUE_CAP; i++) {
        if (s->jobs[i].state == INTERNAL_JOB_FREE) {
            job_index = i;
            break;
        }
    }
    if (job_index < 0) return -1;

    S_INTERNAL_JOB *job = &s->jobs[job_index];
    job->state = INTERNAL_JOB_QUEUED;
    job->rc = -1;
    job->ecm_len = ecm_len;
    memcpy(job->ecm, ecm, ecm_len);
    memset(job->cw, 0, sizeof(job->cw));
    job->waiters = 1;
    s->queue_slots[s->queue_tail] = job_index;
    s->queue_tail = (s->queue_tail + 1) % TCMG_INTERNAL_ECM_QUEUE_CAP;
    s->queue_count++;
    return job_index;
}

static int queue_pop_locked(S_INTERNAL_SLOT *s)
{
    if (s->queue_count <= 0) return -1;
    int job_index = s->queue_slots[s->queue_head];
    s->queue_slots[s->queue_head] = -1;
    s->queue_head = (s->queue_head + 1) % TCMG_INTERNAL_ECM_QUEUE_CAP;
    s->queue_count--;
    return job_index;
}

static void queue_release_locked(S_INTERNAL_JOB *job)
{
    if (!job) return;
    job->state = INTERNAL_JOB_FREE;
    job->rc = -1;
    job->ecm_len = 0;
    job->waiters = 0;
    memset(job->ecm, 0, sizeof(job->ecm));
    memset(job->cw, 0, sizeof(job->cw));
}

static int queue_waiter_count_locked(const S_INTERNAL_SLOT *s)
{
    int total = 0;
    for (int i = 0; i < TCMG_INTERNAL_ECM_QUEUE_CAP; i++)
        total += (int)s->jobs[i].waiters;
    return total;
}

static void slot_close_fd(S_INTERNAL_SLOT *s)
{
    if (!s || s->fd < 0) return;
    if (s->exclusive) (void)flock(s->fd, LOCK_UN);
    close(s->fd);
    s->fd = -1;
    s->exclusive = 0;
}

static void slot_clear(S_INTERNAL_SLOT *s)
{
    slot_close_fd(s);
    s->params_applied = 0;
    s->present = 0;
    s->ready = 0;
    s->protocol = 0;
    s->reset_requested = 0;
    s->last_reset_ms = 0;
    queue_reset_locked(s);
    s->atr_len = 0;
    memset(s->atr, 0, sizeof(s->atr));
    s->device[0] = '\0';
}

static void slot_reset_state(S_INTERNAL_SLOT *s)
{
    s->fd = -1;
    s->exclusive = 0;
    s->params_applied = 0;
    s->present = 0;
    s->ready = 0;
    s->protocol = 0;
    s->reset_requested = 0;
    s->last_reset_ms = 0;
    s->worker_running = 0;
    s->worker_stop = 0;
    queue_reset_locked(s);
    s->atr_len = 0;
    s->device[0] = '\0';
    s->target_device[0] = '\0';
}

static int configure_tty(int fd, const char *device)
{
    if (device && strncmp(device, "/dev/sci", 8) == 0) return 0;
    struct termios tio;
    if (tcgetattr(fd, &tio) < 0) return -1;
    cfmakeraw(&tio);
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag &= ~(CSIZE | CRTSCTS);
    tio.c_cflag |= CS8;
    tio.c_cflag |= PARENB;
    tio.c_cflag &= ~PARODD;
    tio.c_cflag &= ~CSTOPB;
    cfsetispeed(&tio, B9600);
    cfsetospeed(&tio, B9600);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 1;
    if (tcsetattr(fd, TCSANOW, &tio) < 0) return -1;
    tcflush(fd, TCIOFLUSH);
    return 0;
}

static int wait_readable(int fd, int timeout_ms)
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
    int rc;
    do { rc = select(fd + 1, &rfds, NULL, NULL, &tv); } while (rc < 0 && errno == EINTR);
    return rc > 0 ? 0 : -1;
}

static int wait_writable(int fd, int timeout_ms)
{
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(fd, &wfds);
    struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
    int rc;
    do { rc = select(fd + 1, NULL, &wfds, NULL, &tv); } while (rc < 0 && errno == EINTR);
    return rc > 0 ? 0 : -1;
}

static int write_full_timeout(int fd, const uint8_t *buf, size_t len, int timeout_ms)
{
    size_t off = 0;
    while (off < len) {
        if (wait_writable(fd, timeout_ms) < 0) return -1;
        ssize_t n = write(fd, buf + off, len - off);
        if (n > 0) off += (size_t)n;
        else if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        else return -1;
    }
    return 0;
}

static int read_full_timeout(int fd, uint8_t *buf, size_t len, int timeout_ms)
{
    size_t off = 0;
    while (off < len) {
        if (wait_readable(fd, timeout_ms) < 0) return -1;
        ssize_t n = read(fd, buf + off, len - off);
        if (n > 0) off += (size_t)n;
        else if (n < 0 && errno == EINTR) continue;
        else return -1;
    }
    return 0;
}

static int parse_atr_protocol(const uint8_t *atr, size_t len)
{
    if (!atr || len < 2) return -1;
    uint8_t t0 = atr[1];
    size_t p = 2;
    int t = 0;
    uint8_t td = t0;
    for (int group = 1; group <= 8 && p < len; group++) {
        if (td & 0x10) { if (++p > len) return -1; }
        if (td & 0x20) { if (++p > len) return -1; }
        if (td & 0x40) { if (++p > len) return -1; }
        if (td & 0x80) {
            if (p >= len) return -1;
            td = atr[p++];
            t = td & 0x0F;
            continue;
        }
        t = 0;
        break;
    }
    return t;
}

static int atr_read(S_INTERNAL_SLOT *s)
{
    size_t n = 0;
    if (read_full_timeout(s->fd, s->atr, 2, 1000) < 0) return -1;
    n = 2;

    uint8_t tdi = s->atr[1];
    int groups = 1;
    int has_non_t0 = 0;
    while (groups <= 8) {
        if (tdi & 0x10) { if (n >= sizeof(s->atr) || read_full_timeout(s->fd, s->atr + n, 1, 500) < 0) return -1; n++; }
        if (tdi & 0x20) { if (n >= sizeof(s->atr) || read_full_timeout(s->fd, s->atr + n, 1, 500) < 0) return -1; n++; }
        if (tdi & 0x40) { if (n >= sizeof(s->atr) || read_full_timeout(s->fd, s->atr + n, 1, 500) < 0) return -1; n++; }
        if (!(tdi & 0x80)) break;
        if (n >= sizeof(s->atr) || read_full_timeout(s->fd, s->atr + n, 1, 500) < 0) return -1;
        tdi = s->atr[n++] & 0x0F;
        if (tdi != 0) has_non_t0 = 1;
        groups++;
    }
    if (groups > 8 && (tdi & 0x80)) return -1;

    size_t hist = s->atr[1] & 0x0F;
    if (n + hist + (has_non_t0 ? 1u : 0u) > sizeof(s->atr)) return -1;
    if (hist && read_full_timeout(s->fd, s->atr + n, hist, 750) < 0) return -1;
    n += hist;
    if (has_non_t0 && read_full_timeout(s->fd, s->atr + n, 1, 750) < 0) return -1;
    if (has_non_t0) n++;

    s->atr_len = n;
    s->protocol = parse_atr_protocol(s->atr, s->atr_len);
    return s->protocol >= 0 ? 0 : -1;
}

static int sci_apply_params(S_INTERNAL_SLOT *s, int protocol)
{
#ifdef __linux__
    if (s->fd < 0) return -1;
    if (strncmp(s->device, "/dev/sci", 8) != 0) return 0;
    if (s->params_applied && s->protocol == protocol) return 0;
    SCI_PARAMETERS p;
    memset(&p, 0, sizeof(p));
    if (ioctl(s->fd, SCI_GET_PARAMETERS, &p) != 0)
        memset(&p, 0, sizeof(p));
    p.T = (unsigned char)((protocol == 1) ? 1 : 0);
    p.fs = 27;
    p.ETU = 372;
    p.EGT = 0;
    if (ioctl(s->fd, SCI_SET_PARAMETERS, &p) < 0) {
        tcmg_log_dbg(D_READER, "SCI parameters unavailable on %s; using driver defaults", s->device);
        s->params_applied = 0;
        return 0;
    }
    s->params_applied = 1;
    return 0;
#else
    (void)s; (void)protocol;
    return -1;
#endif
}

static int sci_fast_reset(S_INTERNAL_SLOT *s)
{
    if (!s || s->fd < 0) return -1;
#ifdef __linux__
    /* The SCI driver needs its clock/ETU profile programmed before reset.
     * Internal ECM currently uses T=0, so keep the reset profile deterministic. */
    if (sci_apply_params(s, 0) < 0) {
        tcmg_log_dbg(D_READER, "SCI setup failed device=%s", s->device);
        return -1;
    }
    tcflush(s->fd, TCIOFLUSH);
    tcmg_sleep_ms(50);
    uint32_t one = 1;
    if (ioctl(s->fd, SCI_SET_RESET, &one) < 0) {
        tcmg_log("fast reset ioctl failed device=%s errno=%d", s->device, errno);
        s->ready = 0;
        return -1;
    }
#else
    (void)s;
    return -1;
#endif

    s->atr_len = 0;
    s->ready = 0;
    if (atr_read(s) < 0) {
        tcmg_log("fast reset ATR failed device=%s", s->device);
        return -1;
    }
#ifdef __linux__
    uint32_t ready = 1;
    if (ioctl(s->fd, SCI_SET_ATR_READY, &ready) < 0) {
        tcmg_log("fast reset ATR_READY failed device=%s errno=%d", s->device, errno);
        return -1;
    }
#endif
    s->ready = 1;
    s->present = 1;
    if (strncmp(s->device, "/dev/sci", 8) == 0) {
        tcmg_sleep_ms(150);
        s->params_applied = 0;
        (void)sci_apply_params(s, s->protocol);
        tcmg_sleep_ms(150);
    }
    s->last_reset_ms = tcmg_mono_ms();
    tcmg_log_force("fast reset ok device=%s T=%d ATR=%zu",
                  s->device, s->protocol, s->atr_len);
    return 0;
}

static int reader_present(S_INTERNAL_SLOT *s)
{
#ifdef __linux__
    uint32_t present = 0;
    if (ioctl(s->fd, SCI_GET_PRESENT, &present) == 0) {
        s->present = present ? 1 : 0;
        return s->present;
    }
    s->present = 0;
    s->ready = 0;
    s->atr_len = 0;
    tcmg_log_dbg(D_READER, "present poll failed device=%s errno=%d", s->device, errno);
    return -1;
#else
    return s->present;
#endif
}

static int slot_open(S_INTERNAL_SLOT *s, const char *device)
{
    if (!device || !*device) return -1;
    if (s->fd >= 0 && strcmp(s->device, device) == 0) return 0;
    if (s->fd >= 0) close(s->fd);
    s->fd = -1;
    s->params_applied = 0;
    s->present = 0;
    s->ready = 0;
    s->protocol = 0;
    s->reset_requested = 0;
    s->last_reset_ms = 0;
    s->atr_len = 0;
    s->device[0] = '\0';

    int fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return -1;

    int exclusive = 0;
#if defined(TIOCEXCL)
    if (ioctl(fd, TIOCEXCL) == 0) exclusive = 1;
#endif
    if (!exclusive && flock(fd, LOCK_EX | LOCK_NB) == 0) exclusive = 1;

    if (configure_tty(fd, device) < 0) {
        if (exclusive) (void)flock(fd, LOCK_UN);
        close(fd);
        return -1;
    }

    if (!exclusive)
        tcmg_log_dbg(D_READER, "exclusive lock unavailable device=%s; process retains fd ownership", device);

    s->fd = fd;
    s->exclusive = exclusive;
    tcmg_strlcpy(s->device, device, sizeof(s->device));
    return 0;
}

static int internal_recover(S_INTERNAL_SLOT *s, int fast)
{
    if (!s || s->fd < 0) return -1;
    if (fast) return sci_fast_reset(s);

    char device[sizeof(s->device)];
    tcmg_strlcpy(device, s->device, sizeof(device));
    slot_close_fd(s);
    s->params_applied = 0;
    s->present = 0;
    s->ready = 0;
    s->protocol = 0;
    s->atr_len = 0;
    if (slot_open(s, device) < 0) return -1;
    return sci_fast_reset(s);
}

static int t0_transmit(S_INTERNAL_SLOT *s, const uint8_t *apdu, size_t apdu_len,
                       uint8_t *rsp, size_t *rsp_len)
{
    if (!s || s->fd < 0 || !apdu || apdu_len < 5 || apdu_len > 260 || !rsp || !rsp_len)
        return -1;
    if (s->protocol != 0) return -2;
    if (*rsp_len < 2) return -3;

    const uint8_t ins = apdu[1];
    const size_t p3 = apdu[4];
    const int case2 = apdu_len == 5;
    size_t out = 0;

    if (write_full_timeout(s->fd, apdu, 5, 1500) < 0) return -4;

    uint8_t proc = 0;
    for (;;) {
        if (read_full_timeout(s->fd, &proc, 1, 1500) < 0) return -5;
        if (proc == 0x60) continue;
        break;
    }

    if (case2) {
        size_t le = p3 ? p3 : 256u;
        if (le + 2u > *rsp_len) return -6;

        if (proc != ins && proc != (uint8_t)(ins ^ 0xFFu)) return -7;
        if (read_full_timeout(s->fd, rsp, le + 2u, 2000) < 0) return -8;
        out = le + 2u;
    } else {
        const size_t lc = p3;
        if (lc > apdu_len - 5u) return -9;

        if (lc > 0) {
            if (proc == ins) {
                if (write_full_timeout(s->fd, apdu + 5, lc, 1500) < 0) return -10;
            } else if (proc == (uint8_t)(ins ^ 0xFFu)) {
                if (write_full_timeout(s->fd, apdu + 5, 1, 1500) < 0) return -11;
                size_t sent = 1;
                while (sent < lc) {
                    if (read_full_timeout(s->fd, &proc, 1, 1500) < 0) return -12;
                    if (proc == 0x60) continue;
                    if (proc == ins) {
                        if (write_full_timeout(s->fd, apdu + 5 + sent, lc - sent, 1500) < 0) return -13;
                        sent = lc;
                    } else if (proc == (uint8_t)(ins ^ 0xFFu)) {
                        if (write_full_timeout(s->fd, apdu + 5 + sent, 1, 1500) < 0) return -14;
                        sent++;
                    } else {
                        return -15;
                    }
                }
            } else {
                return -16;
            }
        } else if (proc != ins && proc != (uint8_t)(ins ^ 0xFFu)) {
            return -17;
        }

        int first = 1;
        for (;;) {
            int timeout = first ? 1500 : TCMG_T0_RESPONSE_GAP_MS;
            if (wait_readable(s->fd, timeout) < 0) break;
            uint8_t b;
            ssize_t n = read(s->fd, &b, 1);
            if (n == 1) {
                if (out >= *rsp_len) return -18;
                rsp[out++] = b;
                first = 0;
                continue;
            }
            if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
            break;
        }
    }

    if (out < 2) return -19;
    *rsp_len = out;
    return 0;
}

static int internal_parse_conax_cw(const uint8_t *rsp, size_t rsp_len,
                                    uint8_t cw[16], int *found_mask)
{
    if (!rsp || rsp_len < 2 || !cw || !found_mask) return -1;
    *found_mask = 0;

    if (rsp_len >= 3 && rsp[0] == 0x81 && ((rsp[2] >> 5) == 2))
        return -2;

    const size_t data_len = rsp_len - 2;
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

static int internal_conax_ecm(S_INTERNAL_SLOT *s, const uint8_t *ecm, size_t ecm_len,
                              uint8_t cw[16], int *needs_recovery)
{
    if (needs_recovery) *needs_recovery = 0;
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

    if (t0_transmit(s, apdu, apdu_len, rsp, &rsp_len) < 0) {
        if (needs_recovery) *needs_recovery = 1;
        return -2;
    }
    if (rsp_len < 2) return -3;

    uint8_t sw1 = rsp[rsp_len - 2];
    uint8_t sw2 = rsp[rsp_len - 1];
    if (sw1 == 0x90 && sw2 == 0x11) {
        if (needs_recovery) *needs_recovery = 1;
        return -4;
    }
    if (sw1 != 0x90 && sw1 != 0x98) {
        if (needs_recovery) *needs_recovery = 1;
        return -5;
    }

    int got = 0;
    if (sw1 == 0x90 && sw2 == 0x00) {
        if (internal_parse_conax_cw(rsp, rsp_len, cw, &got) == -2) return -6;
    }

    while (sw1 == 0x98 && sw2 != 0x00 && sw2 != 0xFF) {
        uint8_t ins_ca[5] = { 0xDD, 0xCA, 0x00, 0x00, sw2 };
        rsp_len = sizeof(rsp);
        if (t0_transmit(s, ins_ca, sizeof(ins_ca), rsp, &rsp_len) < 0) {
            if (needs_recovery) *needs_recovery = 1;
            return -7;
        }
        if (rsp_len < 2) return -8;

        sw1 = rsp[rsp_len - 2];
        sw2 = rsp[rsp_len - 1];
        if (sw1 == 0x98 || (sw1 == 0x90 && sw2 == 0x00)) {
            int part = 0;
            if (internal_parse_conax_cw(rsp, rsp_len, cw, &part) == -2) return -9;
            got |= part;
        } else {
            return -10;
        }
    }

    return got == 3 ? 0 : -11;
}

static int worker_wait(S_INTERNAL_SLOT *s, int timeout_ms)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }
    pthread_mutex_lock(&s->mtx);
    if (!s->worker_stop && s->queue_count == 0 && !s->reset_requested)
        (void)pthread_cond_timedwait(&s->cv, &s->mtx, &ts);
    int stop = s->worker_stop;
    pthread_mutex_unlock(&s->mtx);
    return stop ? -1 : 0;
}

static void *internal_reader_worker(void *arg)
{
    int index = (int)(intptr_t)arg;
    S_INTERNAL_SLOT *s = &s_slots[index];

    for (;;) {
        S_READER cfg;
        if (!cfg_runtime_reader_get(index, &cfg) ||
            !cfg.in_use || !cfg.enabled ||
            strcasecmp(cfg.protocol, "internal") != 0 || !cfg.device[0])
            break;

        pthread_mutex_lock(&s->mtx);
        if (s->worker_stop) {
            pthread_mutex_unlock(&s->mtx);
            break;
        }
        tcmg_strlcpy(s->target_device, cfg.device, sizeof(s->target_device));
        int fd_ready = (s->fd >= 0 && strcmp(s->device, cfg.device) == 0);
        pthread_mutex_unlock(&s->mtx);

        if (!fd_ready) {
            pthread_mutex_lock(&s->mtx);
            if (!s->worker_stop && slot_open(s, cfg.device) < 0) {
                s->present = 0;
                s->ready = 0;
            }
            pthread_mutex_unlock(&s->mtx);
            if (s->fd < 0) {
                worker_wait(s, 1000);
                continue;
            }
        }

        pthread_mutex_lock(&s->mtx);
        if (s->worker_stop) {
            pthread_mutex_unlock(&s->mtx);
            break;
        }

        int present = reader_present(s);
        if (!present) {
            s->ready = 0;
            s->params_applied = 0;
        } else {
            int recovery = s->reset_requested && s->ready;
            int64_t now_ms = tcmg_mono_ms();
            int interval_due = cfg.fast_reset > 0 && s->ready && s->present &&
                               s->last_reset_ms > 0 &&
                               now_ms - s->last_reset_ms >= (int64_t)cfg.fast_reset * 1000LL;
            int need_reset = !s->ready || s->reset_requested || interval_due;
            if (need_reset) {
                s->reset_requested = 0;
                int rc;
                if (recovery) rc = internal_recover(s, cfg.fast_reset > 0);
                else rc = sci_fast_reset(s);
                if (rc < 0)
                    tcmg_log("reader[%d]: %s reset failed device=%s",
                             index + 1, recovery && cfg.fast_reset <= 0 ? "full" : "fast", s->device);
            }
        }

        int job_index = (s->ready && s->present) ? queue_pop_locked(s) : -1;
        if (job_index >= 0) {
            S_INTERNAL_JOB *job = &s->jobs[job_index];
            if (job->state == INTERNAL_JOB_CANCELLED) {
                queue_release_locked(job);
                pthread_cond_broadcast(&s->cv);
                pthread_mutex_unlock(&s->mtx);
                continue;
            }
            job->state = INTERNAL_JOB_RUNNING;
            size_t ecm_len = job->ecm_len;
            uint8_t ecm[249];
            memcpy(ecm, job->ecm, ecm_len);
            pthread_mutex_unlock(&s->mtx);

            uint8_t cw[16] = {0};
            int needs_recovery = 0;
            int rc = internal_conax_ecm(s, ecm, ecm_len, cw, &needs_recovery);

            pthread_mutex_lock(&s->mtx);
            if (job->state == INTERNAL_JOB_CANCELLED) {
                queue_release_locked(job);
            } else {
                job->rc = rc;
                if (rc == 0) memcpy(job->cw, cw, sizeof(job->cw));
                job->state = INTERNAL_JOB_DONE;
                if (rc < 0 && needs_recovery) s->reset_requested = 1;
                if (job->waiters == 0) queue_release_locked(job);
            }
            pthread_cond_broadcast(&s->cv);
            pthread_mutex_unlock(&s->mtx);
            continue;
        }
        pthread_mutex_unlock(&s->mtx);

        int poll_ms = cfg.poll_ms > 0 ? cfg.poll_ms : 250;
        if (poll_ms < 50) poll_ms = 50;
        if (poll_ms > 10000) poll_ms = 10000;
        if (worker_wait(s, poll_ms) < 0) break;
    }

    pthread_mutex_lock(&s->mtx);
    for (int i = 0; i < TCMG_INTERNAL_ECM_QUEUE_CAP; i++) {
        if (s->jobs[i].state != INTERNAL_JOB_FREE && s->jobs[i].state != INTERNAL_JOB_DONE) {
            s->jobs[i].rc = -9;
            s->jobs[i].state = INTERNAL_JOB_DONE;
        }
    }
    s->worker_running = 0;
    s->worker_stop = 0;
    s->target_device[0] = '\0';
    pthread_cond_broadcast(&s->cv);
    while (queue_waiter_count_locked(s) > 0)
        (void)pthread_cond_wait(&s->cv, &s->mtx);
    slot_clear(s);
    pthread_mutex_unlock(&s->mtx);
    return NULL;
}

static int start_reader_worker(int index, const char *device)
{
    S_INTERNAL_SLOT *s = &s_slots[index];
    pthread_mutex_lock(&s->mtx);
    if (s->worker_running) {
        pthread_mutex_unlock(&s->mtx);
        return 0;
    }
    s->worker_stop = 0;
    queue_reset_locked(s);
    tcmg_strlcpy(s->target_device, device ? device : "", sizeof(s->target_device));
    s->worker_running = 1;
    if (pthread_create(&s->worker_tid, NULL, internal_reader_worker, (void *)(intptr_t)index) != 0) {
        s->worker_running = 0;
        s->target_device[0] = '\0';
        pthread_mutex_unlock(&s->mtx);
        return -1;
    }
    pthread_mutex_unlock(&s->mtx);
    return 0;
}

static void stop_reader_worker(int index)
{
    S_INTERNAL_SLOT *s = &s_slots[index];
    pthread_t tid;
    int join = 0;
    pthread_mutex_lock(&s->mtx);
    if (s->worker_running) {
        s->worker_stop = 1;
        tid = s->worker_tid;
        join = 1;
        pthread_cond_broadcast(&s->cv);
    }
    pthread_mutex_unlock(&s->mtx);
    if (join) pthread_join(tid, NULL);
    pthread_mutex_lock(&s->mtx);
    if (s->fd < 0) slot_reset_state(s);
    pthread_mutex_unlock(&s->mtx);
}

static int internal_sync_once(void)
{
    S_READER cfg_readers[MAX_READERS];
    int reader_count = cfg_runtime_reader_snapshot_indexed(cfg_readers, MAX_READERS);

    for (int i = 0; i < MAX_READERS; i++) {
        S_READER *cfg = (i < reader_count) ? &cfg_readers[i] : NULL;
        int want = cfg && cfg->in_use && cfg->enabled &&
                   strcasecmp(cfg->protocol, "internal") == 0 && cfg->device[0];
        pthread_mutex_lock(&s_slots[i].mtx);
        int running = s_slots[i].worker_running;
        int matches = running && strcmp(s_slots[i].target_device, want ? cfg->device : "") == 0;
        pthread_mutex_unlock(&s_slots[i].mtx);

        if (!want) {
            if (running) stop_reader_worker(i);
            continue;
        }

        if (!matches) {
            if (running) stop_reader_worker(i);
            if (start_reader_worker(i, cfg->device) < 0)
                tcmg_log("reader[%d]: failed to start internal worker device=%s", i + 1, cfg->device);
        }
    }
    return 250;
}

static void *internal_thread(void *arg)
{
    (void)arg;
    while (atomic_load(&s_running)) {
        int poll = internal_sync_once();
        tcmg_sleep_ms(poll);
    }
    return NULL;
}

int internal_start(void)
{
    slots_init();
    if (atomic_exchange(&s_running, 1)) return 0;
    (void)internal_sync_once();
    if (pthread_create(&s_tid, NULL, internal_thread, NULL) != 0) {
        atomic_store(&s_running, 0);
        for (int i = 0; i < MAX_READERS; i++) stop_reader_worker(i);
        return -1;
    }
    tcmg_log("reader support enabled");
    return 0;
}

void internal_stop(void)
{
    slots_init();
    if (atomic_exchange(&s_running, 0)) pthread_join(s_tid, NULL);
    for (int i = 0; i < MAX_READERS; i++) stop_reader_worker(i);
}

int internal_reader_get(int index, S_INTERNAL_READER *out)
{
    if (!out || index < 0 || index >= MAX_READERS) return -1;
    slots_init();
    pthread_mutex_lock(&s_slots[index].mtx);
    memset(out, 0, sizeof(*out));
    out->owned = s_slots[index].fd >= 0;
    out->exclusive = s_slots[index].exclusive;
    tcmg_strlcpy(out->device,
                  s_slots[index].target_device[0] ? s_slots[index].target_device : s_slots[index].device,
                  sizeof(out->device));
    out->present = s_slots[index].present;
    out->ready = s_slots[index].ready;
    out->atr_len = s_slots[index].atr_len;
    memcpy(out->atr, s_slots[index].atr, out->atr_len);
    out->protocol = s_slots[index].protocol;
    int owned = out->owned;
    pthread_mutex_unlock(&s_slots[index].mtx);
    return owned ? 0 : -1;
}

int internal_reader_count(void)
{
    int n = 0;
    slots_init();
    for (int i = 0; i < MAX_READERS; i++) {
        pthread_mutex_lock(&s_slots[i].mtx);
        if (s_slots[i].fd >= 0) n++;
        pthread_mutex_unlock(&s_slots[i].mtx);
    }
    return n;
}

int internal_do_ecm_reader(int index, uint16_t caid,
                           const uint8_t *ecm, size_t ecm_len,
                           uint8_t cw[16], int32_t whitelist)
{
    if (index < 0 || index >= MAX_READERS || !ecm || !cw || ecm_len == 0 || ecm_len > 249)
        return -1;
    if ((caid & 0xFF00u) != 0x0B00u) return -2;
    if (whitelist > 0 && ecm_len > (size_t)whitelist) return -3;

    slots_init();
    S_READER cfg;
    if (!cfg_runtime_reader_get(index, &cfg)) return -4;
    if (!cfg.enabled || strcasecmp(cfg.protocol, "internal") != 0 || !cfg.do_ecm) return -5;

    S_INTERNAL_SLOT *s = &s_slots[index];
    pthread_mutex_lock(&s->mtx);
    if (!s->worker_running || s->fd < 0 || !s->ready || !s->present) {
        pthread_mutex_unlock(&s->mtx);
        return -6;
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += TCMG_INTERNAL_ECM_WAIT_MS / 1000;
    ts.tv_nsec += (long)(TCMG_INTERNAL_ECM_WAIT_MS % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }

    int joined = 0;
    int job_index = -1;
    for (;;) {
        job_index = queue_submit_locked(s, ecm, ecm_len, &joined);
        if (job_index >= 0) break;
        int wait_rc = pthread_cond_timedwait(&s->cv, &s->mtx, &ts);
        if (wait_rc == ETIMEDOUT || !s->worker_running || s->worker_stop) {
            pthread_mutex_unlock(&s->mtx);
            return -7;
        }
        if (!s->ready || !s->present) {
            pthread_mutex_unlock(&s->mtx);
            return -6;
        }
    }

    S_INTERNAL_JOB *job = &s->jobs[job_index];
    if (joined) {
        tcmg_log_dbg(D_READER, "internal reader coalesced ECM index=%d device=%s waiters=%u",
                     index, s->device, job->waiters);
    }
    pthread_cond_signal(&s->cv);

    while (job->state != INTERNAL_JOB_DONE && s->worker_running && !s->worker_stop) {
        int wait_rc = pthread_cond_timedwait(&s->cv, &s->mtx, &ts);
        if (wait_rc == ETIMEDOUT) {
            if (job->waiters > 0) job->waiters--;
            if (job->state == INTERNAL_JOB_QUEUED && job->waiters == 0)
                job->state = INTERNAL_JOB_CANCELLED;
            pthread_cond_broadcast(&s->cv);
            pthread_mutex_unlock(&s->mtx);
            return -8;
        }
    }

    if (job->state != INTERNAL_JOB_DONE) {
        if (job->waiters > 0) job->waiters--;
        if (job->state == INTERNAL_JOB_CANCELLED && job->waiters == 0)
            queue_release_locked(job);
        pthread_cond_broadcast(&s->cv);
        pthread_mutex_unlock(&s->mtx);
        return -9;
    }

    int rc = job->rc;
    if (rc == 0) memcpy(cw, job->cw, sizeof(job->cw));
    if (job->waiters > 0) job->waiters--;
    if (job->waiters == 0) queue_release_locked(job);
    pthread_cond_broadcast(&s->cv);
    pthread_mutex_unlock(&s->mtx);
    return rc;
}

#else

int internal_start(void) { return 0; }
void internal_stop(void) { }
int internal_reader_get(int index, S_INTERNAL_READER *out)
{
    (void)index;
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    return -1;
}
int internal_reader_count(void) { return 0; }
int internal_do_ecm_reader(int index, uint16_t caid,
                           const uint8_t *ecm, size_t ecm_len,
                           uint8_t cw[16], int32_t whitelist)
{
    (void)index; (void)caid; (void)ecm; (void)ecm_len;
    (void)cw; (void)whitelist;
    return -100;
}

#endif
