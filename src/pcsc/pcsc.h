#ifndef TCMG_PCSC_H_
#define TCMG_PCSC_H_

#include "../core/compat.h"
#include "../core/constants.h"

#define TCMG_PCSC_MAX_READERS       32
#define TCMG_PCSC_READER_NAME_MAX   256
#define TCMG_PCSC_ATR_MAX           64

typedef struct {
    char     name[TCMG_PCSC_READER_NAME_MAX];
    int8_t   present;
    uint8_t  atr[TCMG_PCSC_ATR_MAX];
    uint32_t atr_len;
    uint32_t protocol;
} S_PCSC_READER;

int  pcsc_start(void);
void pcsc_stop(void);
int  pcsc_available(void);
int  pcsc_reader_count(void);
int  pcsc_reader_get(int index, S_PCSC_READER *out);
int  pcsc_transmit(const char *reader, const uint8_t *cmd, size_t cmd_len,
                   uint8_t *rsp, size_t *rsp_len);
int  pcsc_do_ecm_reader(const char *selector, uint16_t caid, const uint8_t *ecm, size_t ecm_len, uint8_t cw[CW_LEN], int32_t whitelist);

#endif
