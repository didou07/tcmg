#ifndef TCMG_READER_TYPES_H_
#define TCMG_READER_TYPES_H_

#include "account_types.h"
#include "ident_types.h"

typedef struct {
    char     label[READER_LABEL_LEN];
    char     protocol[READER_PROTOCOL_LEN];
    bool     enabled;
    int8_t   in_use;
    char     device[CFGVAL_LEN];
    char     user[CFGKEY_LEN];
    char     password[CFGKEY_LEN];
    int32_t  inactivitytimeout;
    uint16_t caids[MAX_CAIDS_PER_READER];
    int32_t  ncaids;
    S_IDENT_FILTER idents[MAX_IDENT_FILTERS];
    int32_t  nidents;
    uint16_t sid_whitelist[MAX_SID_WHITELIST];
    int32_t  nsid_whitelist;
    int32_t  ecm_whitelist;
    int32_t  groups[MAX_GROUPS_PER_READER];
    int32_t  ngroups;
    S_ECMKEY keys[MAX_ECMKEYS_PER_ACC];
    int32_t  nkeys;
    int32_t  fast_reset;
    int32_t  fast_reset_idle;
    int32_t  poll_ms;
    uint8_t  newcamd_key[14];
} S_READER;

#endif
