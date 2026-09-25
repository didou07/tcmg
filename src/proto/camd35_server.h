#ifndef TCMG_CAMD35_SERVER_H_
#define TCMG_CAMD35_SERVER_H_

#include <stdint.h>

int32_t cs378x_start(void);
void    cs378x_stop(void);
void   *handle_cs378x_client(void *arg);

#endif
