#ifndef TCMG_AES128_H_
#define TCMG_AES128_H_

#include <stddef.h>
#include <stdint.h>

void aes128_ecb_encrypt(const uint8_t key[16], uint8_t *data, size_t len);
void aes128_ecb_decrypt(const uint8_t key[16], uint8_t *data, size_t len);

#endif
