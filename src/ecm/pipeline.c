#define MODULE_LOG_PREFIX "ecm-pipeline"
#include "pipeline.h"
#include "request.h"
#include "core/config_state.h"
#include "core/utils.h"
#include "platform/platform.h"
#include "cache/cw_cache.h"
#include "log/log.h"
#include "reader/reader.h"
#include "stats/account_stats.h"
#include "srvid/srvid.h"
#include "crypto/crypto.h"
#include "security/antishare.h"
#include "emu/emu.h"

int32_t ecm_process(S_CLIENT *client, uint16_t caid, uint16_t sid, uint32_t provid,
                    const uint8_t *ecm, int32_t ecm_len, uint8_t cw[CW_LEN],
                    S_ECM_RESULT *result)
{
    S_ECM_REQUEST request;
    uint8_t ecm_md5[TCMG_ECM_MD5_LEN];
    S_READER_RESULT reader_result;
    int32_t res = -1;
    bool cache_hit = false;
    int64_t start;
    long elapsed;
    reader_result_init(&reader_result);

    if (result) memset(result, 0, sizeof(*result));
    if (!client || !client->auth.account || !ecm || ecm_len <= 0 || ecm_len > 255 || !cw)
        return -1;

    int was_prepared = client->ecm.antishare_prepared;
    int anti_delay_ms = was_prepared ? client->ecm.antishare_delay_ms : 0;
    client->ecm.antishare_delay_ms = 0;
    client->ecm.antishare_prepared = 0;
    if (client->auth.account->anti_share && !was_prepared) {
        if (antishare_check_request(client->auth.account, client->identity.thread_id, caid, sid, &anti_delay_ms) != AS_CHECK_OK)
            return -2;
    }

    ecm_request_init(&request, client, caid, sid, provid, ecm, ecm_len, cw);
    client->ecm.last_ecm_time = time(NULL);
    client->ecm.last_caid = caid;
    client->ecm.last_srvid = sid;
    srvid_lookup_copy(caid, sid, client->ecm.last_channel, sizeof(client->ecm.last_channel));

    if (D_ECM & g_dblevel)
        log_ecm_raw(caid, sid, ecm, ecm_len);

    crypt_md5_hash(ecm, (size_t)ecm_len, ecm_md5);
    memset(cw, 0, CW_LEN);
    start = tcmg_mono_ms();
    cache_hit = cw_cache_lookup(ecm_md5, cw, request.account, true);

    if (cache_hit) {
        res = EMU_OK;
    } else {
        res = reader_dispatch_ecm(&request, &reader_result);
        if (res == EMU_OK)
            cw_cache_store_groups(ecm_md5, cw, reader_result.groups, reader_result.ngroups);
    }

    elapsed = (long)tcmg_elapsed_ms(start);
    if (res == EMU_OK) {
        antishare_record_success(request.account, client->identity.thread_id, caid, sid, cw);
        if (anti_delay_ms > 0) tcmg_sleep_ms(anti_delay_ms);
    } else {
        antishare_record_failure(request.account, client->identity.thread_id, caid, sid);
    }
    account_stats_record_ecm(request.account, res == EMU_OK, elapsed);

    log_cw_result(caid, sid, ecm_len, cw, res == EMU_OK, cache_hit,
                  (int32_t)elapsed, request.user);
    secure_zero(ecm_md5, sizeof(ecm_md5));
    if (result) {
        result->result = res;
        result->cache_hit = cache_hit;
        result->elapsed_ms = (int32_t)elapsed;
    }
    return res;
}
