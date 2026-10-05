#ifndef TCMG_READER_BACKEND_H
#define TCMG_READER_BACKEND_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef enum {
    TCMG_INTERNAL_BACKEND_NONE = 0,
    TCMG_INTERNAL_BACKEND_SCI,
    TCMG_INTERNAL_BACKEND_STAPI,
    TCMG_INTERNAL_BACKEND_STAPI5,
    TCMG_INTERNAL_BACKEND_AMSMC,
    TCMG_INTERNAL_BACKEND_COOLAPI,
    TCMG_INTERNAL_BACKEND_AZBOX
} E_INTERNAL_BACKEND;

typedef struct {
    E_INTERNAL_BACKEND kind;
    int fd;
    int exclusive;
    void *ctx;
    char name[32];
    char device[256];
} S_INTERNAL_BACKEND;

void internal_backend_init(S_INTERNAL_BACKEND *b);
int internal_backend_open(S_INTERNAL_BACKEND *b, const char *spec);
int internal_backend_present(S_INTERNAL_BACKEND *b);
int internal_backend_reset(S_INTERNAL_BACKEND *b, uint8_t *atr, size_t *atr_len);
int internal_backend_set_clock(S_INTERNAL_BACKEND *b, uint32_t clock_khz);
ssize_t internal_backend_read(S_INTERNAL_BACKEND *b, uint8_t *buf, size_t len, uint32_t timeout_ms);
ssize_t internal_backend_write(S_INTERNAL_BACKEND *b, const uint8_t *buf, size_t len, uint32_t timeout_ms);
int internal_backend_flush(S_INTERNAL_BACKEND *b);
int internal_backend_transceive(S_INTERNAL_BACKEND *b, const uint8_t *tx, size_t tx_len,
                                uint8_t *rx, size_t *rx_len, uint32_t timeout_ms);
int internal_backend_is_open(const S_INTERNAL_BACKEND *b);
int internal_backend_is_fd(const S_INTERNAL_BACKEND *b);
int internal_backend_get_fd(const S_INTERNAL_BACKEND *b);
const char *internal_backend_name(const S_INTERNAL_BACKEND *b);
const char *internal_backend_device(const S_INTERNAL_BACKEND *b);
void internal_backend_close(S_INTERNAL_BACKEND *b);
const char *internal_backend_platform_name(void);

#endif
