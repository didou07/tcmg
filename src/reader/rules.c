#define MODULE_LOG_PREFIX "reader-rules"
#include "rules.h"

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

bool reader_allows(const S_READER *reader, const S_ACCOUNT *acc,
                   uint16_t caid, uint16_t sid, int32_t ecm_len)
{
    if (!reader || !acc || !reader->in_use || !reader->enabled) return false;

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
    if (!group_ok) return false;

    if (reader->ncaids > 0) {
        bool caid_ok = false;
        for (int i = 0; i < reader->ncaids; i++) {
            if (reader->caids[i] == caid) {
                caid_ok = true;
                break;
            }
        }
        if (!caid_ok) return false;
    }

    if (reader->nsid_whitelist > 0) {
        bool sid_ok = false;
        for (int i = 0; i < reader->nsid_whitelist; i++) {
            if (reader->sid_whitelist[i] == sid) {
                sid_ok = true;
                break;
            }
        }
        if (!sid_ok) return false;
    }

    if (reader->ecm_whitelist > 0 && ecm_len > reader->ecm_whitelist)
        return false;

    return true;
}
