#ifndef TCMG_BAN_TYPES_H_
#define TCMG_BAN_TYPES_H_

#include "compat.h"
#include "constants.h"

typedef struct s_ban_entry {
    char    ip[MAXIPLEN];
    int32_t fails;
    time_t  until;
    struct s_ban_entry *next;
} S_BAN_ENTRY;

#endif
