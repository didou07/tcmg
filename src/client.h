#ifndef TCMG_CLIENT_H_
#define TCMG_CLIENT_H_

#include "tcmg_types.h"

void client_register(S_CLIENT *cl);
void client_unregister(S_CLIENT *cl);
void client_kill_by_tid(uint32_t tid);
void clients_relink_accounts(void);
void *handle_client(void *arg);

#endif /* TCMG_CLIENT_H_ */
