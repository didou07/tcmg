#define MODULE_LOG_PREFIX "share"
#include "antishare.h"
#include "../platform/platform.h"
#include "../core/compat.h"
#include <string.h>
#include <stdint.h>

static int64_t window_ms(int seconds)
{
    if (seconds <= 0) return 1000;
    if (seconds > 3600) seconds = 3600;
    return (int64_t)seconds * 1000;
}

static void reset_ecm_window_locked(S_ACCOUNT *acc, int64_t now_ms)
{
    const int64_t ttl = window_ms(acc->as_ecm_window_s);
    if (acc->as_ecm_window_start_ms <= 0 || now_ms - acc->as_ecm_window_start_ms >= ttl) {
        acc->as_ecm_window_start_ms = now_ms;
        acc->as_ecm_points = 0;
    }
}

static void sync_channel_count_locked(S_ACCOUNT *acc);

static int purge_channels_locked(S_ACCOUNT *acc, int64_t now_ms)
{
    const int64_t timeout = window_ms(acc->as_channel_timeout_s);
    uint32_t seen[AS_MAX_CHANNELS];
    int unique = 0;
    for (int i = 0; i < AS_MAX_CHANNELS; i++) {
        if (!acc->as_channels[i].client_tid) continue;
        bool expire = false;
        if (acc->as_channels[i].pending) {
            expire = acc->as_channels[i].pending_since_ms > 0 &&
                     now_ms - acc->as_channels[i].pending_since_ms > 30000;
        } else if (acc->as_channels[i].last_success_ms > 0) {
            expire = now_ms - acc->as_channels[i].last_success_ms >= timeout;
        }
        if (expire) {
            memset(&acc->as_channels[i], 0, sizeof(acc->as_channels[i]));
            continue;
        }
        const uint32_t key = ((uint32_t)acc->as_channels[i].caid << 16) | acc->as_channels[i].sid;
        bool duplicate = false;
        for (int j = 0; j < unique; j++) {
            if (seen[j] == key) { duplicate = true; break; }
        }
        if (!duplicate) seen[unique++] = key;
    }
    acc->as_channel_count = unique;
    return unique;
}

static int find_channel_locked(const S_ACCOUNT *acc, uint32_t client_tid, uint16_t caid, uint16_t sid)
{
    for (int i = 0; i < AS_MAX_CHANNELS; i++) {
        if (acc->as_channels[i].client_tid == client_tid &&
            acc->as_channels[i].caid == caid &&
            acc->as_channels[i].sid == sid)
            return i;
    }
    return -1;
}


static int count_unique_channels_locked(const S_ACCOUNT *acc)
{
    int count = 0;
    for (int i = 0; i < AS_MAX_CHANNELS; i++) {
        if (!acc->as_channels[i].client_tid) continue;
        bool duplicate = false;
        for (int j = 0; j < i; j++) {
            if (acc->as_channels[j].client_tid &&
                acc->as_channels[j].caid == acc->as_channels[i].caid &&
                acc->as_channels[j].sid == acc->as_channels[i].sid) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) count++;
    }
    return count;
}

static void sync_channel_count_locked(S_ACCOUNT *acc)
{
    acc->as_channel_count = count_unique_channels_locked(acc);
}


static void purge_recent_channels_locked(S_ACCOUNT *acc, int64_t now_ms)
{
    const int64_t timeout = window_ms(acc->as_channel_timeout_s);
    int out = 0;
    for (int i = 0; i < AS_MAX_RECENT_CHANNELS; i++) {
        if (!acc->as_recent_channels[i].caid || !acc->as_recent_channels[i].sid) continue;
        if (acc->as_recent_channels[i].last_seen_ms <= 0 ||
            now_ms - acc->as_recent_channels[i].last_seen_ms >= timeout)
            continue;
        if (out != i)
            acc->as_recent_channels[out] = acc->as_recent_channels[i];
        out++;
    }
    for (int i = out; i < AS_MAX_RECENT_CHANNELS; i++)
        memset(&acc->as_recent_channels[i], 0, sizeof(acc->as_recent_channels[i]));
    acc->as_recent_count = out;
}

static int find_recent_channel_locked(const S_ACCOUNT *acc, uint16_t caid, uint16_t sid)
{
    for (int i = 0; i < AS_MAX_RECENT_CHANNELS; i++) {
        if (acc->as_recent_channels[i].caid == caid &&
            acc->as_recent_channels[i].sid == sid)
            return i;
    }
    return -1;
}

static void record_recent_channel_locked(S_ACCOUNT *acc, uint16_t caid, uint16_t sid,
                                         int64_t now_ms)
{
    int idx = find_recent_channel_locked(acc, caid, sid);
    if (idx >= 0) {
        if (acc->as_recent_channels[idx].hits < UINT16_MAX)
            acc->as_recent_channels[idx].hits++;
        acc->as_recent_channels[idx].last_seen_ms = now_ms;
        return;
    }

    if (acc->as_recent_count < AS_MAX_RECENT_CHANNELS) {
        idx = acc->as_recent_count++;
    } else {
        idx = 0;
        for (int i = 1; i < AS_MAX_RECENT_CHANNELS; i++) {
            if (acc->as_recent_channels[i].last_seen_ms < acc->as_recent_channels[idx].last_seen_ms)
                idx = i;
        }
    }
    memset(&acc->as_recent_channels[idx], 0, sizeof(acc->as_recent_channels[idx]));
    acc->as_recent_channels[idx].caid = caid;
    acc->as_recent_channels[idx].sid = sid;
    acc->as_recent_channels[idx].hits = 1;
    acc->as_recent_channels[idx].first_seen_ms = now_ms;
    acc->as_recent_channels[idx].last_seen_ms = now_ms;
}



static void decay_suspicion_locked(S_ACCOUNT *acc, int64_t now_ms)
{
    if (acc->as_suspicion_score <= 0) {
        acc->as_suspicion_last_ms = now_ms;
        return;
    }
    if (acc->as_suspicion_last_ms <= 0) {
        acc->as_suspicion_last_ms = now_ms;
        return;
    }
    const int64_t step_ms = 5000;
    if (now_ms <= acc->as_suspicion_last_ms) return;
    int steps = (int)((now_ms - acc->as_suspicion_last_ms) / step_ms);
    if (steps <= 0) return;
    acc->as_suspicion_score -= steps;
    if (acc->as_suspicion_score < 0) acc->as_suspicion_score = 0;
    acc->as_suspicion_last_ms += (int64_t)steps * step_ms;
}

static void raise_suspicion_locked(S_ACCOUNT *acc, int points, int64_t now_ms)
{
    decay_suspicion_locked(acc, now_ms);
    if (points > 0) {
        acc->as_suspicion_score += points;
        if (acc->as_suspicion_score > 12) acc->as_suspicion_score = 12;
    }
    acc->as_suspicion_last_ms = now_ms;
}

static int suspicion_delay_ms_locked(const S_ACCOUNT *acc)
{
    if (acc->as_switch_delay_s <= 0) return 0;
    const int score = acc->as_suspicion_score;
    if (score < 5) return 0;
    int multiplier = score >= 10 ? 3 : (score >= 8 ? 2 : 1);
    int ms = acc->as_switch_delay_s * 1000 * multiplier;
    if (ms > 5000) ms = 5000;
    return ms;
}

static void update_suspicion_locked(S_ACCOUNT *acc, int old_idx, int same_channel_idx,
                                    int projected_unique, int projected_recent_distinct,
                                    int projected_recent_repeated, int64_t now_ms)
{
    decay_suspicion_locked(acc, now_ms);
    if (old_idx >= 0) {
        const int64_t prev_ms = acc->as_channels[old_idx].last_success_ms > acc->as_channels[old_idx].pending_since_ms
            ? acc->as_channels[old_idx].last_success_ms : acc->as_channels[old_idx].pending_since_ms;
        if (prev_ms > 0) {
            const int64_t dt = now_ms - prev_ms;
            if (dt >= 0 && dt <= 3000) raise_suspicion_locked(acc, 2, now_ms);
        }
    }
    if (same_channel_idx < 0 && projected_unique >= 3) raise_suspicion_locked(acc, 2, now_ms);
    if (projected_recent_distinct >= 3) raise_suspicion_locked(acc, 1, now_ms);
    if (projected_recent_repeated >= 2) raise_suspicion_locked(acc, 1, now_ms);
}

T_ANTISHARE_STATUS antishare_check_channel(S_ACCOUNT *acc, uint32_t client_tid,
                                            uint16_t caid, uint16_t sid, int *delay_ms)
{
    if (delay_ms) *delay_ms = 0;
    if (!acc || !acc->anti_share) return AS_CHECK_OK;

    const int64_t now_ms = tcmg_mono_ms();
    pthread_mutex_lock(&acc->as_mtx);
    const int unique_channels = purge_channels_locked(acc, now_ms);
    purge_recent_channels_locked(acc, now_ms);

    int idx = -1, old_idx = -1, same_idx = -1, free_idx = -1;
    bool same_other_client = false;
    uint32_t seen[AS_MAX_CHANNELS];
    int seen_count = 0;
    for (int i = 0; i < AS_MAX_CHANNELS; i++) {
        const uint32_t tid = acc->as_channels[i].client_tid;
        if (!tid) {
            if (free_idx < 0) free_idx = i;
            continue;
        }
        const uint16_t ch_caid = acc->as_channels[i].caid;
        const uint16_t ch_sid = acc->as_channels[i].sid;
        if (tid == client_tid && old_idx < 0) old_idx = i;
        if (tid == client_tid && ch_caid == caid && ch_sid == sid) idx = i;
        if (same_idx < 0 && ch_caid == caid && ch_sid == sid) same_idx = i;
        if (ch_caid == caid && ch_sid == sid && tid != client_tid) same_other_client = true;
        const uint32_t key = ((uint32_t)ch_caid << 16) | ch_sid;
        bool duplicate = false;
        for (int j = 0; j < seen_count; j++) {
            if (seen[j] == key) { duplicate = true; break; }
        }
        if (!duplicate && seen_count < AS_MAX_CHANNELS) seen[seen_count++] = key;
    }

    const int max_channels = acc->as_max_sids > 0 ? acc->as_max_sids : 1;
    const bool switching_channel = old_idx >= 0 &&
        (acc->as_channels[old_idx].caid != caid || acc->as_channels[old_idx].sid != sid);
    const int64_t old_activity_ms = old_idx >= 0
        ? (acc->as_channels[old_idx].last_success_ms > acc->as_channels[old_idx].pending_since_ms
            ? acc->as_channels[old_idx].last_success_ms : acc->as_channels[old_idx].pending_since_ms)
        : 0;
    const bool rapid_switch = switching_channel && old_activity_ms > 0 &&
        now_ms - old_activity_ms <= 3000;
    bool old_channel_other_client = false;
    if (old_idx >= 0) {
        const uint16_t old_caid = acc->as_channels[old_idx].caid;
        const uint16_t old_sid = acc->as_channels[old_idx].sid;
        for (int i = 0; i < AS_MAX_CHANNELS; i++) {
            if (i != old_idx && acc->as_channels[i].client_tid &&
                acc->as_channels[i].client_tid != client_tid &&
                acc->as_channels[i].caid == old_caid &&
                acc->as_channels[i].sid == old_sid) {
                old_channel_other_client = true;
                break;
            }
        }
    }

    int recent_idx = -1, recent_distinct = 0, recent_repeated = 0;
    for (int i = 0; i < AS_MAX_RECENT_CHANNELS; i++) {
        if (!acc->as_recent_channels[i].caid || !acc->as_recent_channels[i].sid) continue;
        recent_distinct++;
        if (acc->as_recent_channels[i].hits >= 2) recent_repeated++;
        if (acc->as_recent_channels[i].caid == caid && acc->as_recent_channels[i].sid == sid)
            recent_idx = i;
    }
    const bool new_recent_channel = recent_idx < 0;
    const int projected_recent_distinct = recent_distinct + (new_recent_channel ? 1 : 0);
    const int projected_recent_repeated = recent_repeated +
        (recent_idx >= 0 && acc->as_recent_channels[recent_idx].hits == 1 ? 1 : 0);
    int projected_unique = unique_channels;
    if (same_idx < 0 && (old_idx < 0 || same_other_client)) projected_unique++;
    const bool suspicious_fanout = projected_recent_repeated >= 2 || projected_unique >= 3 ||
        (rapid_switch && projected_recent_distinct >= 3);

    if (idx >= 0) {
        record_recent_channel_locked(acc, caid, sid, now_ms);
        decay_suspicion_locked(acc, now_ms);
        if (suspicious_fanout) raise_suspicion_locked(acc, 1, now_ms);
        if (delay_ms) *delay_ms = suspicion_delay_ms_locked(acc);
        pthread_mutex_unlock(&acc->as_mtx);
        return AS_CHECK_OK;
    }

    if (unique_channels >= max_channels && same_idx < 0 &&
        (old_idx < 0 || old_channel_other_client)) {
        pthread_mutex_unlock(&acc->as_mtx);
        return AS_CHECK_CHANNEL_LIMIT;
    }

    if (old_idx >= 0 && unique_channels >= max_channels) idx = old_idx;
    else idx = free_idx;
    if (idx < 0) {
        pthread_mutex_unlock(&acc->as_mtx);
        return AS_CHECK_CHANNEL_LIMIT;
    }

    if (switching_channel || same_idx < 0)
        update_suspicion_locked(acc, old_idx, same_idx, projected_unique,
                                projected_recent_distinct, projected_recent_repeated, now_ms);

    memset(&acc->as_channels[idx], 0, sizeof(acc->as_channels[idx]));
    acc->as_channels[idx].client_tid = client_tid;
    acc->as_channels[idx].caid = caid;
    acc->as_channels[idx].sid = sid;
    acc->as_channels[idx].pending = 1;
    acc->as_channels[idx].pending_since_ms = now_ms;
    acc->as_channel_count = unique_channels +
        ((idx == free_idx && same_idx < 0) ? 1 : 0);
    record_recent_channel_locked(acc, caid, sid, now_ms);
    if (delay_ms) *delay_ms = suspicion_delay_ms_locked(acc);
    pthread_mutex_unlock(&acc->as_mtx);
    return AS_CHECK_OK;
}

T_ANTISHARE_STATUS antishare_begin_ecm(S_ACCOUNT *acc)
{
    if (!acc || !acc->anti_share) return AS_CHECK_OK;

    const int64_t now_ms = tcmg_mono_ms();
    pthread_mutex_lock(&acc->as_mtx);
    reset_ecm_window_locked(acc, now_ms);

    if (acc->as_max_ecm > 0 &&
        acc->as_ecm_points + acc->as_ecm_active >= acc->as_max_ecm) {
        pthread_mutex_unlock(&acc->as_mtx);
        return AS_CHECK_RATE_LIMIT;
    }

    acc->as_ecm_active++;
    pthread_mutex_unlock(&acc->as_mtx);
    return AS_CHECK_OK;
}

void antishare_end_ecm(S_ACCOUNT *acc, bool success)
{
    if (!acc || !acc->anti_share) return;

    const int64_t now_ms = tcmg_mono_ms();
    pthread_mutex_lock(&acc->as_mtx);
    reset_ecm_window_locked(acc, now_ms);
    if (acc->as_ecm_active > 0) acc->as_ecm_active--;
    if (success) acc->as_ecm_points++;
    pthread_mutex_unlock(&acc->as_mtx);
}

void antishare_record_channel_success(S_ACCOUNT *acc, uint32_t client_tid,
                                       uint16_t caid, uint16_t sid, const uint8_t cw[CW_LEN])
{
    if (!acc || !acc->anti_share) return;
    const int64_t now_ms = tcmg_mono_ms();
    pthread_mutex_lock(&acc->as_mtx);
    purge_channels_locked(acc, now_ms);

    const int idx = find_channel_locked(acc, client_tid, caid, sid);
    if (idx >= 0) {
        acc->as_channels[idx].pending = 0;
        acc->as_channels[idx].pending_since_ms = 0;
        acc->as_channels[idx].last_success_ms = now_ms;
    }

    acc->as_last_cw_caid = caid;
    acc->as_last_cw_sid = sid;
    acc->as_last_cw_ms = now_ms;
    if (cw) memcpy(acc->as_last_cw, cw, CW_LEN);
    pthread_mutex_unlock(&acc->as_mtx);
}


void antishare_copy_runtime(S_ACCOUNT *dst, const S_ACCOUNT *src)
{
    if (!dst || !src || dst == src) return;
    pthread_mutex_lock((pthread_mutex_t *)&src->as_mtx);
    memcpy(dst->as_channels, src->as_channels, sizeof(dst->as_channels));
    dst->as_ecm_points = src->as_ecm_points;
    dst->as_ecm_active = 0;
    dst->as_ecm_window_start_ms = src->as_ecm_window_start_ms;
    memcpy(dst->as_last_cw, src->as_last_cw, sizeof(dst->as_last_cw));
    dst->as_last_cw_caid = src->as_last_cw_caid;
    dst->as_last_cw_sid = src->as_last_cw_sid;
    dst->as_last_cw_ms = src->as_last_cw_ms;
    dst->as_channel_count = src->as_channel_count;
    memcpy(dst->as_recent_channels, src->as_recent_channels, sizeof(dst->as_recent_channels));
    dst->as_recent_count = src->as_recent_count;
    dst->as_suspicion_score = src->as_suspicion_score;
    dst->as_suspicion_last_ms = src->as_suspicion_last_ms;
    pthread_mutex_unlock((pthread_mutex_t *)&src->as_mtx);
}

void antishare_clear_pending(S_ACCOUNT *acc, uint32_t client_tid, uint16_t caid, uint16_t sid)
{
    if (!acc || !acc->anti_share) return;
    pthread_mutex_lock(&acc->as_mtx);
    const int idx = find_channel_locked(acc, client_tid, caid, sid);
    if (idx >= 0 && acc->as_channels[idx].pending)
        memset(&acc->as_channels[idx], 0, sizeof(acc->as_channels[idx]));
    sync_channel_count_locked(acc);
    pthread_mutex_unlock(&acc->as_mtx);
}

void antishare_release_client(S_ACCOUNT *acc, uint32_t client_tid)
{
    if (!acc || !client_tid) return;
    pthread_mutex_lock(&acc->as_mtx);
    for (int i = 0; i < AS_MAX_CHANNELS; i++) {
        if (acc->as_channels[i].client_tid == client_tid)
            memset(&acc->as_channels[i], 0, sizeof(acc->as_channels[i]));
    }
    sync_channel_count_locked(acc);
    pthread_mutex_unlock(&acc->as_mtx);
}
