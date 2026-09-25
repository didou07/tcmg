#ifndef TCMG_CLIENT_IDENTITY_H_
#define TCMG_CLIENT_IDENTITY_H_

#include "constants.h"
#include <stdint.h>

#define TCMG_CLIENT_NAME_LEN 32
#define TCMG_CLIENT_PROTO_NAME_LEN 12

typedef struct {
    char ip[MAXIPLEN];
    char user[CFGKEY_LEN];
    char client_name[TCMG_CLIENT_NAME_LEN];
    uint32_t thread_id;
    uint16_t client_id;
} S_CLIENT_IDENTITY;

#endif
