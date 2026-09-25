#ifndef TCMG_ECM_TYPES_H_
#define TCMG_ECM_TYPES_H_

#include "core/client_types.h"
#include "core/account_types.h"
#include "core/constants.h"
#include <stdbool.h>
#include <stdint.h>

#define TCMG_ECM_MD5_LEN 16

typedef enum {
    ECM_ACCESS_OK = 0,
    ECM_ACCESS_NO_ACCOUNT,
    ECM_ACCESS_DISABLED,
    ECM_ACCESS_EXPIRED,
    ECM_ACCESS_SCHEDULE,
    ECM_ACCESS_ANTISHARE,
    ECM_ACCESS_CAID_DENIED,
    ECM_ACCESS_SID_DENIED
} T_ECM_ACCESS_STATUS;

typedef struct {
    int32_t result;
    bool cache_hit;
    int32_t elapsed_ms;
} S_ECM_RESULT;

typedef struct {
    S_CLIENT *client;
    S_ACCOUNT *account;
    int fd;
    uint32_t thread_id;
    const char *user;
    const char *ip;
    uint16_t caid;
    uint16_t sid;
    uint32_t provid;
    const uint8_t *ecm;
    int32_t ecm_len;
    uint8_t *cw;
} S_ECM_REQUEST;

#endif
