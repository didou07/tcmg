#ifndef MODULE_NEWCAMD_H_
#define MODULE_NEWCAMD_H_

#include "globals.h"

#define NCD_DEFAULT_PORT      15050
#define NCD_DEFAULT_KEEPALIVE 0

void  newcamd_start(void);
void  newcamd_stop(void);
void *handle_newcamd_client(void *arg);

#endif
