#ifndef TCMG_CLIENT_TYPES_H_
#define TCMG_CLIENT_TYPES_H_

#include "client_account_state.h"
#include "client_ecm_state.h"
#include "client_identity.h"
#include "client_protocol_state.h"
#include "client_session.h"

#include <stdint.h>

typedef struct s_client {
    S_CLIENT_IDENTITY identity;
    S_CLIENT_SESSION session;
    S_CLIENT_ACCOUNT_STATE auth;
    S_CLIENT_ECM_STATE ecm;
    S_CLIENT_PROTOCOL_STATE protocol;
} S_CLIENT;

typedef struct {
    int fd;
    char ip[MAXIPLEN];
} S_CONN_ARGS;

#endif
