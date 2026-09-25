#ifndef TCMG_ECM_POLICY_H_
#define TCMG_ECM_POLICY_H_

#include "types.h"
#include <stdbool.h>

T_ECM_ACCESS_STATUS ecm_access(S_CLIENT *client, uint16_t caid, uint16_t sid,
                               bool check_schedule, bool check_caid, bool check_sid);

#endif
