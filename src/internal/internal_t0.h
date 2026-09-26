#ifndef TCMG_INTERNAL_T0_H_
#define TCMG_INTERNAL_T0_H_

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int fd;
    uint32_t wwt_ms;
    uint32_t io_write_timeout_ms;
    uint32_t max_nulls;
} S_INTERNAL_T0_CHANNEL;

int internal_t0_exchange(const S_INTERNAL_T0_CHANNEL *ch,
                         const uint8_t *apdu, size_t apdu_len,
                         uint8_t *rsp, size_t *rsp_len);

#endif
