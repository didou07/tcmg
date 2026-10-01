#define MODULE_LOG_PREFIX "reader"
#include "rules.h"
#include "core/config_state.h"

bool reader_account_has_group(const S_ACCOUNT *acc, int32_t group)
{
    if (!acc || group < 1) return false;
    if (acc->ngroups > 0) {
        for (int i = 0; i < acc->ngroups; i++) {
            if (acc->groups[i] == group) return true;
        }
        return false;
    }
    return acc->group == group;
}

int reader_collect_account_caids(const S_ACCOUNT *acc, uint16_t *out, int32_t cap)
{
    int32_t count = 0;

    if (!acc || !out || cap <= 0) return 0;

    pthread_rwlock_rdlock(&g_cfg.acc_lock);
    for (int32_t i = 0; i < MAX_READERS && count < cap; i++) {
        const S_READER *reader = &g_cfg.readers[i];
        if (!reader->in_use || !reader->enabled || reader->ncaids <= 0) continue;

        bool group_ok = false;
        for (int32_t j = 0; j < reader->ngroups && !group_ok; j++) {
            if (reader_account_has_group(acc, reader->groups[j]))
                group_ok = true;
        }
        if (!group_ok) continue;

        for (int32_t j = 0; j < reader->ncaids && count < cap; j++) {
            uint16_t caid = reader->caids[j];
            if (!caid) continue;

            bool exists = false;
            for (int32_t k = 0; k < count; k++) {
                if (out[k] == caid) {
                    exists = true;
                    break;
                }
            }
            if (!exists) out[count++] = caid;
        }
    }
    pthread_rwlock_unlock(&g_cfg.acc_lock);
    return count;
}

E_READER_RULE_RESULT reader_rule_check(const S_READER *reader, const S_ACCOUNT *acc,
                                        uint16_t caid, uint16_t sid, int32_t ecm_len)
{
    if (!reader || !acc || !reader->in_use || !reader->enabled)
        return READER_RULE_DISABLED;

    bool group_ok = false;
    if (reader->ngroups > 0) {
        for (int i = 0; i < reader->ngroups; i++) {
            if (reader_account_has_group(acc, reader->groups[i])) {
                group_ok = true;
                break;
            }
        }
    } else {
        group_ok = reader_account_has_group(acc, 1);
    }
    if (!group_ok) return READER_RULE_GROUP;

    if (reader->ncaids > 0) {
        bool caid_ok = false;
        for (int i = 0; i < reader->ncaids; i++) {
            if (reader->caids[i] == caid) {
                caid_ok = true;
                break;
            }
        }
        if (!caid_ok) return READER_RULE_CAID;
    }

    if (reader->nsid_whitelist > 0) {
        bool sid_ok = false;
        for (int i = 0; i < reader->nsid_whitelist; i++) {
            if (reader->sid_whitelist[i] == sid) {
                sid_ok = true;
                break;
            }
        }
        if (!sid_ok) return READER_RULE_SID;
    }

    if (reader->ecm_whitelist > 0 && ecm_len != reader->ecm_whitelist)
        return READER_RULE_ECM_WHITELIST;

    return READER_RULE_ALLOW;
}

bool reader_allows(const S_READER *reader, const S_ACCOUNT *acc,
                   uint16_t caid, uint16_t sid, int32_t ecm_len)
{
    return reader_rule_check(reader, acc, caid, sid, ecm_len) == READER_RULE_ALLOW;
}
