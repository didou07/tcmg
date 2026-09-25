#ifndef TCMG_CLIENT_ACCOUNT_STATE_H_
#define TCMG_CLIENT_ACCOUNT_STATE_H_

#include "account_types.h"
#include <stdint.h>

typedef struct {
    S_ACCOUNT *account;
    int8_t counted;
} S_CLIENT_ACCOUNT_STATE;

#endif
