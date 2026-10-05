#ifndef TCMG_INTERNAL_T0_H_
#define TCMG_INTERNAL_T0_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef ssize_t (*internal_t0_read_fn)(void *ctx, uint8_t *buf, size_t len, uint32_t timeout_ms);
typedef ssize_t (*internal_t0_write_fn)(void *ctx, const uint8_t *buf, size_t len, uint32_t timeout_ms);
typedef int (*internal_t0_flush_fn)(void *ctx);

typedef struct {
    int fd;
    void *ctx;
    internal_t0_read_fn read_fn;
    internal_t0_write_fn write_fn;
    internal_t0_flush_fn flush_fn;
    uint32_t wwt_ms;
    uint32_t io_write_timeout_ms;
    uint32_t max_nulls;
} S_INTERNAL_T0_CHANNEL;

int internal_t0_exchange(const S_INTERNAL_T0_CHANNEL *ch,
                         const uint8_t *apdu, size_t apdu_len,
                         const char *context,
                         uint8_t *rsp, size_t *rsp_len);

#endif
