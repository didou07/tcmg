#include "cccam_crypto.h"
#include "../crypto/crypto.h"
#include <string.h>

void cccam_crypto_init(S_CC_CRYPT *b, const uint8_t *key, size_t key_len)
{
    if (!b || !key || key_len == 0) return;
    uint8_t j = 0;
    for (size_t i = 0; i < 256; i++) b->keytable[i] = (uint8_t)i;
    for (size_t i = 0; i < 256; i++) {
        j = (uint8_t)(j + key[i % key_len] + b->keytable[i]);
        uint8_t tmp = b->keytable[i];
        b->keytable[i] = b->keytable[j];
        b->keytable[j] = tmp;
    }
    b->state = key[0];
    b->counter = 0;
    b->sum = 0;
}

void cccam_crypto_crypt(S_CC_CRYPT *b, uint8_t *data, size_t len, bool encrypt)
{
    if (!b || !data) return;
    for (size_t i = 0; i < len; i++) {
        b->counter++;
        b->sum = (uint8_t)(b->sum + b->keytable[b->counter]);
        uint8_t tmp = b->keytable[b->counter];
        b->keytable[b->counter] = b->keytable[b->sum];
        b->keytable[b->sum] = tmp;
        const uint8_t input = data[i];
        data[i] = (uint8_t)(input ^ b->keytable[(b->keytable[b->counter] + b->keytable[b->sum]) & 0xFF] ^ b->state);
        b->state ^= encrypt ? input : data[i];
    }
}

void cccam_crypto_seed_xor(uint8_t buf[CCCAM_SEED_LEN])
{
    static const uint8_t ccstr[6] = {'C','C','c','a','m',0};
    if (!buf) return;
    for (uint8_t i = 0; i < 8; i++) {
        buf[i + 8] = (uint8_t)(i * buf[i]);
        if (i <= 5) buf[i] ^= ccstr[i];
    }
}

void cccam_crypto_cw(uint8_t cw[16], uint32_t card_id, const uint8_t node_id[8])
{
    if (!cw || !node_id) return;
    uint8_t nod[8];
    for (int i = 0; i < 8; i++) nod[i] = node_id[7 - i];
    for (int i = 0; i < 16; i++) {
        const int j = i >> 1;
        uint8_t n;
        if (i & 1) {
            if (i != 15)
                n = (uint8_t)(((uint16_t)nod[j] >> 4) | ((uint16_t)nod[j + 1] << 4));
            else
                n = (uint8_t)(nod[j] >> 4);
        } else {
            n = nod[j];
        }
        uint8_t tmp = (uint8_t)(cw[i] ^ n);
        if (i & 1) tmp = (uint8_t)~tmp;
        cw[i] = (uint8_t)(((card_id >> (2 * i)) ^ tmp) & 0xFF);
    }
    secure_zero(nod, sizeof(nod));
}
