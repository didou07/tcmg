#ifndef TCMG_CLIENT_H_
#define TCMG_CLIENT_H_

#include "../core/client_types.h"
#include "../core/client_state.h"
#include "../config/config.h"

void client_init(S_CLIENT *cl, int fd, const char *ip);
void client_register(S_CLIENT *cl);
void client_unregister(S_CLIENT *cl);
void client_kill_by_tid(uint32_t tid);
void client_kill_by_user(const char *username);
void clients_relink_accounts(void);

#endif
