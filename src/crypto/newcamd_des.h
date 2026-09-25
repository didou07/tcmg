#ifndef TCMG_NEWCAMD_DES_H_
#define TCMG_NEWCAMD_DES_H_

#include <stdint.h>

#define DES_IP    1
#define DES_IP_1  2
#define DES_RIGHT 4
#define DES_HASH  8

int32_t tcmg_ncd_des_encrypt(uint8_t *buffer, int len, uint8_t *deskey);
int32_t tcmg_ncd_des_decrypt(uint8_t *buffer, int len, uint8_t *deskey);
void tcmg_ncd_des_login_key_get(const uint8_t *key1, const uint8_t *key2, int len, uint8_t *des16);

#endif
