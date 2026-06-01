#ifndef TCMG_CRYPTO_H_
#define TCMG_CRYPTO_H_

void crypt_init(void);
void crypt_des_enc(const uint8_t *key8, const uint8_t *in8, uint8_t *out8);
void crypt_des_dec(const uint8_t *key8, const uint8_t *in8, uint8_t *out8);
void crypt_key_spread(const uint8_t *key14, uint8_t *out16);
void crypt_ede2_cbc(const uint8_t *key16, const uint8_t *iv,
                    const uint8_t *in, uint8_t *out, size_t len, bool encrypt);
bool crypt_md5_crypt(const char *pw, const char *salt, char *out, size_t outsz);
void crypt_md5_hash(const uint8_t *data, size_t len, uint8_t out[16]);

bool csprng(uint8_t *buf, size_t len);
bool ct_memeq(const uint8_t *a, const uint8_t *b, size_t n);
bool ct_streq(const char *a, const char *b);
void secure_zero(void *ptr, size_t len);

#endif /* TCMG_CRYPTO_H_ */
