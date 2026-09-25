#ifndef TCMG_CONF_H_
#define TCMG_CONF_H_

#include <stdbool.h>
#include <stddef.h>
#include "../core/config_types.h"

bool        cfg_load(const char *file, S_CONFIG *cfg);
bool        cfg_save(S_CONFIG *cfg);
bool        cfg_reload(const char *file, char *errbuf, size_t errsz);

S_ACCOUNT  *cfg_account_new(S_CONFIG *cfg);
void        cfg_accounts_free(S_CONFIG *cfg);

S_READER   *cfg_reader_new(S_CONFIG *cfg, int index);

void        cfg_print(const S_CONFIG *cfg);
const char *cfg_client_name(uint16_t client_id);
bool        cfg_write_default(const char *path);

#endif
