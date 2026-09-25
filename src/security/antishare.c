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
    int64_t ttl = window_ms(acc->as_ecm_window_s);
    if (acc->as_ecm_window_start_ms <= 0 || now_ms - acc->as_ecm_window_start_ms >= ttl) {
        acc->as_ecm_window_start_ms = now_ms;
        acc->as_ecm_count = 0;
    }
}

static void purge_channels_locked(S_ACCOUNT *acc, int64_t now_ms)
{
    int64_t timeout = window_ms(acc->as_channel_timeout_s);
    for (int i = 0; i < AS_MAX_CHANNELS; i++) {
        if (!acc->as_channels[i].client_tid) continue;

        if (acc->as_channels[i].pending) {
            if (acc->as_channels[i].pending_since_ms > 0 &&
                now_ms - acc->as_channels[i].pending_since_ms > 30000)
                memset(&acc->as_channels[i], 0, sizeof(acc->as_channels[i]));
            continue;
        }

        if (acc->as_channels[i].last_success_ms > 0 &&
            now_ms - acc->as_channels[i].last_success_ms >= timeout)
            memset(&acc->as_channels[i], 0, sizeof(acc->as_channels[i]));
    }
}

static int occupied_channels_locked(const S_ACCOUNT *acc)
{
    int n = 0;
    for (int i = 0; i < AS_MAX_CHANNELS; i++)
        if (acc->as_channels[i].client_tid) n++;
    return n;
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

static int find_client_channel_locked(const S_ACCOUNT *acc, uint32_t client_tid)
{
    for (int i = 0; i < AS_MAX_CHANNELS; i++)
        if (acc->as_channels[i].client_tid == client_tid) return i;
    return -1;
}

static int find_free_locked(const S_ACCOUNT *acc)
{
    for (int i = 0; i < AS_MAX_CHANNELS; i++)
        if (!acc->as_channels[i].client_tid) return i;
    return -1;
}

T_ANTISHARE_STATUS antishare_check_request(S_ACCOUNT *acc, uint32_t client_tid,
                                            uint16_t caid, uint16_t sid, int *delay_ms)
{
    if (delay_ms) *delay_ms = 0;
    if (!acc || !acc->anti_share) return AS_CHECK_OK;

    const int64_t now_ms = tcmg_mono_ms();
    pthread_mutex_lock(&acc->as_mtx);
    reset_ecm_window_locked(acc, now_ms);
    purge_channels_locked(acc, now_ms);

    /* Count every accepted ECM request, not only successful ECMs. */
    if (acc->as_max_ecm > 0 && acc->as_ecm_count >= acc->as_max_ecm) {
        pthread_mutex_unlock(&acc->as_mtx);
        return AS_CHECK_ECM_RATE;
    }

    int idx = find_channel_locked(acc, client_tid, caid, sid);
    if (idx >= 0) {
        /* Repeating ECMs on the same active/pending channel do not consume another slot. */
        acc->as_ecm_count++;
        pthread_mutex_unlock(&acc->as_mtx);
        return AS_CHECK_OK;
    }

    /* Keep the existing channel for this client available as the replacement slot. */
    const int old_idx = find_client_channel_locked(acc, client_tid);
    const int max_channels = acc->as_max_sids > 0 ? acc->as_max_sids : 1;
    if (occupied_channels_locked(acc) >= max_channels && old_idx < 0) {
        pthread_mutex_unlock(&acc->as_mtx);
        return AS_CHECK_ACTIVE_CHANNELS;
    }

    idx = old_idx >= 0 && occupied_channels_locked(acc) >= max_channels ? old_idx : find_free_locked(acc);
    if (idx < 0) {
        pthread_mutex_unlock(&acc->as_mtx);
        return AS_CHECK_ACTIVE_CHANNELS;
    }

    const int switching_channel = old_idx >= 0 &&
        (acc->as_channels[old_idx].caid != caid || acc->as_channels[old_idx].sid != sid);

    memset(&acc->as_channels[idx], 0, sizeof(acc->as_channels[idx]));
    acc->as_channels[idx].client_tid = client_tid;
    acc->as_channels[idx].caid = caid;
    acc->as_channels[idx].sid = sid;
    acc->as_channels[idx].pending = 1;
    acc->as_channels[idx].pending_since_ms = now_ms;
    acc->as_ecm_count++;

    /* A channel switch by the same client can optionally delay the returned CW. */
    if (switching_channel && delay_ms && acc->as_switch_delay_s > 0)
        *delay_ms = acc->as_switch_delay_s * 1000;

    pthread_mutex_unlock(&acc->as_mtx);
    return AS_CHECK_OK;
}

void antishare_record_success(S_ACCOUNT *acc, uint32_t client_tid, uint16_t caid,
                              uint16_t sid, const uint8_t cw[CW_LEN])
{
    if (!acc || !acc->anti_share) return;
    const int64_t now_ms = tcmg_mono_ms();
    pthread_mutex_lock(&acc->as_mtx);
    reset_ecm_window_locked(acc, now_ms);
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

void antishare_record_failure(S_ACCOUNT *acc, uint32_t client_tid, uint16_t caid, uint16_t sid)
{
    if (!acc || !acc->anti_share) return;
    pthread_mutex_lock(&acc->as_mtx);
    const int idx = find_channel_locked(acc, client_tid, caid, sid);
    if (idx >= 0 && acc->as_channels[idx].pending)
        memset(&acc->as_channels[idx], 0, sizeof(acc->as_channels[idx]));
    pthread_mutex_unlock(&acc->as_mtx);
}

void antishare_release_client(S_ACCOUNT *acc, uint32_t client_tid)
{
    if (!acc || !client_tid) return;
    pthread_mutex_lock(&acc->as_mtx);
    for (int i = 0; i < AS_MAX_CHANNELS; i++)
        if (acc->as_channels[i].client_tid == client_tid)
            memset(&acc->as_channels[i], 0, sizeof(acc->as_channels[i]));
    pthread_mutex_unlock(&acc->as_mtx);
}
