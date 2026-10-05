#ifndef TCMG_CCCAM_CRYPTO_H_
#define TCMG_CCCAM_CRYPTO_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "cccam.h"

void cccam_crypto_init(S_CC_CRYPT *block, const uint8_t *key, size_t key_len);
void cccam_crypto_crypt(S_CC_CRYPT *block, uint8_t *data, size_t len, bool encrypt);
void cccam_crypto_seed_xor(uint8_t buf[CCCAM_SEED_LEN]);
void cccam_crypto_cw(uint8_t cw[16], uint32_t card_id, const uint8_t node_id[8]);

#endif
