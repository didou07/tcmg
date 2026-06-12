#ifndef TCMG_NEWCAMD_H_
#define TCMG_NEWCAMD_H_

#define NCD_DEFAULT_PORT      15050
#define NCD_DEFAULT_KEEPALIVE 0

int32_t newcamd_start(void);
void    newcamd_stop(void);
void   *handle_newcamd_client(void *arg);

#endif
