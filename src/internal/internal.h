#ifndef TCMG_INTERNAL_H_
#define TCMG_INTERNAL_H_

#include <stddef.h>
#include <stdint.h>

#define TCMG_INTERNAL_MAX_ATR 64

typedef struct {
    char device[256];
    int present;
    int ready;
    uint8_t atr[TCMG_INTERNAL_MAX_ATR];
    size_t atr_len;
    int protocol;
} S_INTERNAL_READER;

int internal_start(void);
void internal_stop(void);
int internal_reader_get(int index, S_INTERNAL_READER *out);
int internal_reader_count(void);
int internal_do_ecm_reader(int index, uint16_t caid,
                           const uint8_t *ecm, size_t ecm_len,
                           uint8_t cw[16], int32_t whitelist);

#endif
