#ifndef TCMG_READER_RULES_H_
#define TCMG_READER_RULES_H_

#include "core/account_types.h"
#include "core/reader_types.h"
#include <stdbool.h>
#include <stdint.h>

bool reader_account_has_group(const S_ACCOUNT *acc, int32_t group);
bool reader_allows(const S_READER *reader, const S_ACCOUNT *acc,
                   uint16_t caid, uint16_t sid, int32_t ecm_len);

#endif
