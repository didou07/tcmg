#define MODULE_LOG_PREFIX "stats"
#include "stats.h"

#include <stdatomic.h>
#include <time.h>

#define READER_ACTIVE_WINDOW_S 8

static _Atomic int64_t s_cw_ok[MAX_READERS];
static _Atomic int64_t s_cw_nok[MAX_READERS];
static _Atomic int64_t s_last_ok[MAX_READERS];

void reader_stats_record(int index, bool success)
{
    if (index < 0 || index >= MAX_READERS) return;
    if (success) {
        atomic_fetch_add_explicit(&s_cw_ok[index], 1, memory_order_relaxed);
        atomic_store_explicit(&s_last_ok[index], (int64_t)time(NULL), memory_order_release);
    } else {
        atomic_fetch_add_explicit(&s_cw_nok[index], 1, memory_order_relaxed);
    }
}

void reader_stats_snapshot(int index, S_READER_STATS_SNAPSHOT *out)
{
    int64_t last_ok, now;
    if (!out) return;
    out->cw_ok = 0;
    out->cw_nok = 0;
    out->active = 0;
    if (index < 0 || index >= MAX_READERS) return;

    out->cw_ok = atomic_load_explicit(&s_cw_ok[index], memory_order_relaxed);
    out->cw_nok = atomic_load_explicit(&s_cw_nok[index], memory_order_relaxed);
    last_ok = atomic_load_explicit(&s_last_ok[index], memory_order_acquire);
    now = (int64_t)time(NULL);
    out->active = last_ok > 0 && now >= last_ok && now - last_ok <= READER_ACTIVE_WINDOW_S;
}

void reader_stats_reset(int index)
{
    if (index < 0 || index >= MAX_READERS) return;
    atomic_store_explicit(&s_cw_ok[index], 0, memory_order_relaxed);
    atomic_store_explicit(&s_cw_nok[index], 0, memory_order_relaxed);
    atomic_store_explicit(&s_last_ok[index], 0, memory_order_release);
}
