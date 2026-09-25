#ifndef TCMG_PROTO_SERVER_H_
#define TCMG_PROTO_SERVER_H_

#include "core/client_types.h"

typedef void *(*T_PROTO_CLIENT_HANDLER)(void *arg);

typedef struct {
    const char *name;
    int fd;
    pthread_t thread;
    _Atomic int32_t running;
    T_PROTO_CLIENT_HANDLER client_handler;
} S_PROTO_SERVER;

int32_t proto_server_start(S_PROTO_SERVER *server,
                           const char *name,
                           int32_t port,
                           const char *bindaddr,
                           T_PROTO_CLIENT_HANDLER client_handler);
void    proto_server_stop(S_PROTO_SERVER *server);

#endif
