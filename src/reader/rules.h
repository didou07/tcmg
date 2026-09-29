#ifndef TCMG_READER_RULES_H_
#define TCMG_READER_RULES_H_

#include "core/account_types.h"
#include "core/reader_types.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    READER_RULE_ALLOW = 0,
    READER_RULE_DISABLED,
    READER_RULE_GROUP,
    READER_RULE_CAID,
    READER_RULE_SID,
    READER_RULE_ECM_WHITELIST,
} E_READER_RULE_RESULT;

bool reader_account_has_group(const S_ACCOUNT *acc, int32_t group);
int reader_collect_account_caids(const S_ACCOUNT *acc, uint16_t *out, int32_t cap);
E_READER_RULE_RESULT reader_rule_check(const S_READER *reader, const S_ACCOUNT *acc,
                                        uint16_t caid, uint16_t sid, int32_t ecm_len);
bool reader_allows(const S_READER *reader, const S_ACCOUNT *acc,
                   uint16_t caid, uint16_t sid, int32_t ecm_len);

#endif
