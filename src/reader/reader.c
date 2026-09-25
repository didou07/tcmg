#define MODULE_LOG_PREFIX "reader"
#include "reader.h"
#include "core/config_state.h"
#include "ecm/request.h"
#include "core/utils.h"
#include "log/log.h"
#include "protocol.h"
#include "rules.h"
#include "result.h"

int32_t reader_dispatch_ecm(const S_ECM_REQUEST *request, S_READER_RESULT *result)
{
    S_READER readers[MAX_READERS];
    int indices[MAX_READERS];
    int n = 0;

    if (result) reader_result_init(result);
    if (!request || !request->account || !request->ecm || !request->cw ||
        request->ecm_len <= 0 || request->ecm_len > 255)
        return -1;

    pthread_rwlock_rdlock(&g_cfg.acc_lock);
    for (int i = 0; i < MAX_READERS; i++) {
        if (!g_cfg.readers[i].in_use) continue;
        readers[n] = g_cfg.readers[i];
        indices[n] = i;
        n++;
    }
    pthread_rwlock_unlock(&g_cfg.acc_lock);

    for (int i = 0; i < n; i++) {
        if (!reader_allows(&readers[i], request->account,
                           request->caid, request->sid, request->ecm_len)) continue;

        const S_READER_PROTOCOL *protocol = reader_protocol_find(readers[i].protocol);
        if (!protocol || !protocol->do_ecm) {
            tcmg_log_dbg(D_READER, "reader index=%d label='%s' unknown protocol=%s",
                         indices[i], readers[i].label, readers[i].protocol);
            continue;
        }

        S_READER_ECM_REQUEST call = {
            .index = indices[i],
            .reader = &readers[i],
            .request = request,
        };
        int rc = protocol->do_ecm(&call);
        if (rc == EMU_OK) {
            if (result) {
                int ngrp = readers[i].ngroups;
                result->status = EMU_OK;
                result->reader_index = indices[i];
                if (ngrp <= 0) {
                    result->groups[0] = 1;
                    result->ngroups = 1;
                } else {
                    if (ngrp > MAX_GROUPS_PER_READER) ngrp = MAX_GROUPS_PER_READER;
                    memcpy(result->groups, readers[i].groups,
                           (size_t)ngrp * sizeof(result->groups[0]));
                    result->ngroups = ngrp;
                }
            }
            return EMU_OK;
        }

        tcmg_log_dbg(D_READER,
                     "reader skip/failed index=%d label='%s' protocol=%s caid=%04X sid=%04X rc=%d",
                     indices[i], readers[i].label, readers[i].protocol,
                     request->caid, request->sid, rc);
    }

    return -2;
}

void reader_shutdown(void)
{
    for (size_t i = 0; i < reader_protocol_count(); i++) {
        const S_READER_PROTOCOL *protocol = reader_protocol_at(i);
        if (protocol && protocol->shutdown) protocol->shutdown();
    }
}
