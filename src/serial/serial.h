#ifndef TCMG_SERIAL_READER_H_
#define TCMG_SERIAL_READER_H_

#include <stddef.h>
#include <stdint.h>

#define TCMG_SERIAL_MAX_ATR 64
#define TCMG_SERIAL_MAX_PORTS 64
#define TCMG_SERIAL_PORT_LEN 256

typedef struct {
    char device[TCMG_SERIAL_PORT_LEN];
    int present;
    int ready;
    uint8_t atr[TCMG_SERIAL_MAX_ATR];
    size_t atr_len;
    int protocol;
} S_SERIAL_READER;

int serial_start(void);
void serial_stop(void);
int serial_reader_get(int index, S_SERIAL_READER *out);
int serial_reader_count(void);
int serial_do_ecm_reader(int index, uint16_t caid,
                         const uint8_t *ecm, size_t ecm_len,
                         uint8_t cw[16], int32_t whitelist);

size_t serial_list_ports(char out[][TCMG_SERIAL_PORT_LEN], size_t max_ports);

#endif
