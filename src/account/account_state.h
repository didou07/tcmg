#ifndef TCMG_ACCOUNT_STATE_H_
#define TCMG_ACCOUNT_STATE_H_

#include "../core/account_types.h"
#include <stdint.h>

void account_state_read_lock(void);
void account_state_read_unlock(void);
void account_state_write_lock(void);
void account_state_write_unlock(void);
S_ACCOUNT *account_state_list_locked(void);
int32_t account_state_count_locked(void);

#endif
