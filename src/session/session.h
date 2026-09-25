#ifndef TCMG_SESSION_H_
#define TCMG_SESSION_H_

#include "core/client_types.h"
#include <stdbool.h>
#include <time.h>

typedef struct {
    int fd;
    const char *ip;
} T_SESSION_INPUT;

void session_init(S_CLIENT *client, const T_SESSION_INPUT *input, const char *proto);
bool session_idle_expired(const S_CLIENT *client, time_t now);
void session_cleanup(S_CLIENT *client);

#endif
