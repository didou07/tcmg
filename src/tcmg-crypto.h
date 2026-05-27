/*
 * tcmg-crypto.h — DES-EDE2-CBC, MD5-crypt, key spread.
 *
 * Pure C, no libssl dependency.  All state is stack-allocated.
 * Key material is zeroed immediately after use via secure_zero().
 */

#ifndef TCMG_CRYPTO_H_
#define TCMG_CRYPTO_H_

/* One-time init (currently a no-op; reserved for future platform hooks). */
void crypt_init(void);

/* DES single-block encrypt/decrypt (8-byte key, 8-byte block). */
void crypt_des_enc(const uint8_t *key8, const uint8_t *in8, uint8_t *out8);
void crypt_des_dec(const uint8_t *key8, const uint8_t *in8, uint8_t *out8);

/*
 * 14-byte → 16-byte Newcamd key spread with odd-parity fixup.
 */
void crypt_key_spread(const uint8_t *key14, uint8_t *out16);

/*
 * DES-EDE2-CBC over 'len' bytes (must be multiple of 8).
 * key16[0..7] = K1, key16[8..15] = K2.
 * iv is consumed + modified in-place per block.
 */
void crypt_ede2_cbc(const uint8_t *key16, const uint8_t *iv,
                     const uint8_t *in, uint8_t *out,
                     size_t len, bool encrypt);

/*
 * MD5-crypt — password hash used by Newcamd login.
 * out must be ≥ 34 bytes.  Returns false on malformed salt.
 */
bool crypt_md5_crypt(const char *pw, const char *salt, char *out, size_t outsz);

/* Internal MD5 block transform (exposed for testing). */
void crypt_md5_hash(const uint8_t *data, size_t len, uint8_t out[16]);

/* Constant-time helpers (prevent timing side-channels). */
bool ct_memeq(const uint8_t *a, const uint8_t *b, size_t n);
bool ct_streq(const char *a, const char *b);

/* Cryptographically secure random bytes. */
bool csprng(uint8_t *buf, size_t len);

/* Wipe sensitive memory. */
void secure_zero(void *ptr, size_t len);

#endif /* TCMG_CRYPTO_H_ */
