#define MODULE_LOG_PREFIX "stats"
#include "stats.h"
#include "../platform/platform.h"

#include <stdatomic.h>

#define READER_ACTIVE_WINDOW_S 8

static _Atomic uint32_t s_cw_ok[MAX_READERS];
static _Atomic uint32_t s_cw_nok[MAX_READERS];
static _Atomic uint32_t s_last_ok_s[MAX_READERS];

void reader_stats_record(int index, bool success)
{
    if (index < 0 || index >= MAX_READERS) return;
    if (success) {
        atomic_fetch_add_explicit(&s_cw_ok[index], 1, memory_order_relaxed);
        atomic_store_explicit(&s_last_ok_s[index], (uint32_t)(tcmg_mono_ms32() / 1000u), memory_order_release);
    } else {
        atomic_fetch_add_explicit(&s_cw_nok[index], 1, memory_order_relaxed);
    }
}

void reader_stats_snapshot(int index, S_READER_STATS_SNAPSHOT *out)
{
    uint32_t last_ok, now;
    if (!out) return;
    out->cw_ok = 0;
    out->cw_nok = 0;
    out->active = 0;
    if (index < 0 || index >= MAX_READERS) return;

    out->cw_ok = atomic_load_explicit(&s_cw_ok[index], memory_order_relaxed);
    out->cw_nok = atomic_load_explicit(&s_cw_nok[index], memory_order_relaxed);
    last_ok = atomic_load_explicit(&s_last_ok_s[index], memory_order_acquire);
    now = (uint32_t)(tcmg_mono_ms32() / 1000u);
    out->active = last_ok != 0 && (uint32_t)(now - last_ok) <= READER_ACTIVE_WINDOW_S;
}

void reader_stats_reset(int index)
{
    if (index < 0 || index >= MAX_READERS) return;
    atomic_store_explicit(&s_cw_ok[index], 0, memory_order_relaxed);
    atomic_store_explicit(&s_cw_nok[index], 0, memory_order_relaxed);
    atomic_store_explicit(&s_last_ok_s[index], 0, memory_order_release);
}
