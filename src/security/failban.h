#ifndef TCMG_FAILBAN_H_
#define TCMG_FAILBAN_H_

#include "../core/compat.h"
uint32_t ban_hash_pub(const char *ip);
bool ban_is_banned(const char *ip);
void ban_record_fail(const char *ip);
void ban_record_ok(const char *ip);
void ban_free_all(void);

#endif
