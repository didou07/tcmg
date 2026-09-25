#ifndef TCMG_ACCOUNT_H_
#define TCMG_ACCOUNT_H_

#include "core/account_types.h"
#include "core/client_types.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ACCOUNT_OK = 0,
    ACCOUNT_DISABLED,
    ACCOUNT_EXPIRED,
    ACCOUNT_IP_DENIED,
    ACCOUNT_MAX_CONNECTIONS
} T_ACCOUNT_STATUS;

S_ACCOUNT *account_acquire(const char *user);
void account_retain(S_ACCOUNT *account);
void account_release(S_ACCOUNT *account);
void account_retire(S_ACCOUNT *account);
void account_reap_retired(void);
void account_retired_free(void);

T_ACCOUNT_STATUS account_validate(const S_ACCOUNT *account, const char *ip);
void account_mark_login(S_ACCOUNT *account, const char *ip);

int account_session_open(S_CLIENT *client, S_ACCOUNT *account);
void account_session_close(S_CLIENT *client);
void account_session_rebind(S_CLIENT *client, S_ACCOUNT *account);

bool account_allows_caid(const S_ACCOUNT *account, uint16_t caid);
bool account_allows_sid(const S_ACCOUNT *account, uint16_t sid);
bool account_in_schedule(const S_ACCOUNT *account);

#endif
