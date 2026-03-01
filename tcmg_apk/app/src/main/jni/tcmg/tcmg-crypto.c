
#define MODULE_LOG_PREFIX "crypto"
#include "tcmg-globals.h"

/* Windows: BCryptGenRandom — must link with -lbcrypt */
#if defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__) || defined(__MINGW64__)
#  include <bcrypt.h>
#else
#  include <fcntl.h>
#  if defined(__linux__) && defined(__GLIBC__) && \
      (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 25))
#    include <sys/random.h>
#  endif
#endif

bool csprng(uint8_t *buf, size_t len)
{
#if defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__) || defined(__MINGW64__)
	NTSTATUS st = BCryptGenRandom(NULL, (PUCHAR)buf, (ULONG)len,
	                              BCRYPT_USE_SYSTEM_PREFERRED_RNG);
	return BCRYPT_SUCCESS(st);
#elif defined(__linux__) && defined(__GLIBC__) && \
      (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 25))
	ssize_t n = getrandom(buf, len, 0);
	return n == (ssize_t)len;
#else
	static int urfd = -1;
	if (urfd < 0)
		urfd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
	if (urfd < 0) return false;
	ssize_t n = read(urfd, buf, len);
	return n == (ssize_t)len;
#endif
}

bool ct_memeq(const uint8_t *a, const uint8_t *b, size_t n)
{
	uint8_t diff = 0;
	size_t  i;
	for (i = 0; i < n; i++) diff |= a[i] ^ b[i];
	return diff == 0;
}

bool ct_streq(const char *a, const char *b)
{
	size_t la = strlen(a), lb = strlen(b);
	if (la != lb) return false;
	return ct_memeq((const uint8_t *)a, (const uint8_t *)b, la);
}

void secure_zero(void *ptr, size_t len)
{
	volatile uint8_t *p = (volatile uint8_t *)ptr;
	size_t i;
	for (i = 0; i < len; i++) p[i] = 0;
}

static const int IP[64] = {
	58,50,42,34,26,18,10,2, 60,52,44,36,28,20,12,4,
	62,54,46,38,30,22,14,6, 64,56,48,40,32,24,16,8,
	57,49,41,33,25,17,9,1,  59,51,43,35,27,19,11,3,
	61,53,45,37,29,21,13,5, 63,55,47,39,31,23,15,7
};
static const int FP[64] = {
	40,8,48,16,56,24,64,32, 39,7,47,15,55,23,63,31,
	38,6,46,14,54,22,62,30, 37,5,45,13,53,21,61,29,
	36,4,44,12,52,20,60,28, 35,3,43,11,51,19,59,27,
	34,2,42,10,50,18,58,26, 33,1,41,9,49,17,57,25
};
static const int E[48] = {
	32,1,2,3,4,5, 4,5,6,7,8,9, 8,9,10,11,12,13, 12,13,14,15,16,17,
	16,17,18,19,20,21, 20,21,22,23,24,25, 24,25,26,27,28,29, 28,29,30,31,32,1
};
static const int P[32] = {
	16,7,20,21, 29,12,28,17, 1,15,23,26, 5,18,31,10,
	2,8,24,14, 32,27,3,9, 19,13,30,6, 22,11,4,25
};
static const int PC1[56] = {
	57,49,41,33,25,17,9, 1,58,50,42,34,26,18,
	10,2,59,51,43,35,27, 19,11,3,60,52,44,36,
	63,55,47,39,31,23,15, 7,62,54,46,38,30,22,
	14,6,61,53,45,37,29, 21,13,5,28,20,12,4
};
static const int PC2[48] = {
	14,17,11,24,1,5, 3,28,15,6,21,10,
	23,19,12,4,26,8, 16,7,27,20,13,2,
	41,52,31,37,47,55, 30,40,51,45,33,48,
	44,49,39,56,34,53, 46,42,50,36,29,32
};
static const int SH[16] = {1,1,2,2,2,2,2,2,1,2,2,2,2,2,2,1};
static const int SB[8][64] = {
	{14,4,13,1,2,15,11,8,3,10,6,12,5,9,0,7,0,15,7,4,14,2,13,1,10,6,12,11,9,5,3,8,4,1,14,8,13,6,2,11,15,12,9,7,3,10,5,0,15,12,8,2,4,9,1,7,5,11,3,14,10,0,6,13},
	{15,1,8,14,6,11,3,4,9,7,2,13,12,0,5,10,3,13,4,7,15,2,8,14,12,0,1,10,6,9,11,5,0,14,7,11,10,4,13,1,5,8,12,6,9,3,2,15,13,8,10,1,3,15,4,2,11,6,7,12,0,5,14,9},
	{10,0,9,14,6,3,15,5,1,13,12,7,11,4,2,8,13,7,0,9,3,4,6,10,2,8,5,14,12,11,15,1,13,6,4,9,8,15,3,0,11,1,2,12,5,10,14,7,1,10,13,0,6,9,8,7,4,15,14,3,11,5,2,12},
	{7,13,14,3,0,6,9,10,1,2,8,5,11,12,4,15,13,8,11,5,6,15,0,3,4,7,2,12,1,10,14,9,10,6,9,0,12,11,7,13,15,1,3,14,5,2,8,4,3,15,0,6,10,1,13,8,9,4,5,11,12,7,2,14},
	{2,12,4,1,7,10,11,6,8,5,3,15,13,0,14,9,14,11,2,12,4,7,13,1,5,0,15,10,3,9,8,6,4,2,1,11,10,13,7,8,15,9,12,5,6,3,0,14,11,8,12,7,1,14,2,13,6,15,0,9,10,4,5,3},
	{12,1,10,15,9,2,6,8,0,13,3,4,14,7,5,11,10,15,4,2,7,12,9,5,6,1,13,14,0,11,3,8,9,14,15,5,2,8,12,3,7,0,4,10,1,13,11,6,4,3,2,12,9,5,15,10,11,14,1,7,6,0,8,13},
	{4,11,2,14,15,0,8,13,3,12,9,7,5,10,6,1,13,0,11,7,4,9,1,10,14,3,5,12,2,15,8,6,1,4,11,13,12,3,7,14,10,15,6,8,0,5,9,2,6,11,13,8,1,4,10,7,9,5,0,15,14,2,3,12},
	{13,2,8,4,6,15,11,1,10,9,3,14,5,0,12,7,1,15,13,8,10,3,7,4,12,5,6,11,0,14,9,2,7,11,4,1,9,12,14,2,0,6,10,13,15,3,5,8,2,1,14,7,4,10,8,13,15,12,9,0,3,5,6,11}
};

static uint64_t des_permute64(uint64_t in, const int *tbl, int n)
{
	uint64_t out = 0; int i;
	for (i = 0; i < n; i++)
		if (in & (1ULL << (64 - tbl[i])))
			out |= (1ULL << (n - 1 - i));
	return out;
}
static uint32_t des_permute32(uint32_t in, const int *tbl, int n)
{
	uint32_t out = 0; int i;
	for (i = 0; i < n; i++)
		if (in & (1U << (32 - tbl[i])))
			out |= (1U << (n - 1 - i));
	return out;
}
static void des_subkeys(const uint8_t *key, uint64_t sk[16])
{
	uint64_t key64 = 0; int i, j;
	for (i = 0; i < 8; i++) key64 |= ((uint64_t)key[i] << (56 - i * 8));
	/* PC1 */
	uint64_t perm = 0;
	for (i = 0; i < 56; i++)
		if (key64 & (1ULL << (64 - PC1[i])))
			perm |= (1ULL << (55 - i));
	uint32_t c = (perm >> 28) & 0x0FFFFFFF;
	uint32_t d = perm & 0x0FFFFFFF;
	for (i = 0; i < 16; i++)
	{
		for (j = 0; j < SH[i]; j++)
		{
			c = ((c << 1) | (c >> 27)) & 0x0FFFFFFF;
			d = ((d << 1) | (d >> 27)) & 0x0FFFFFFF;
		}
		uint64_t cd = ((uint64_t)c << 28) | d;
		uint64_t s2 = 0;
		for (j = 0; j < 48; j++)
			if (cd & (1ULL << (56 - PC2[j])))
				s2 |= (1ULL << (47 - j));
		sk[i] = s2;
	}
}
static uint32_t des_f(uint32_t r, uint64_t sk)
{
	uint64_t exp = 0; int i;
	for (i = 0; i < 48; i++)
		if (r & (1U << (32 - E[i])))
			exp |= (1ULL << (47 - i));
	exp ^= sk;
	uint32_t out = 0;
	for (i = 0; i < 8; i++)
	{
		int bi  = (exp >> (42 - i * 6)) & 0x3F;
		int row = ((bi & 0x20) >> 4) | (bi & 1);
		int col = (bi >> 1) & 0x0F;
		out |= (SB[i][row * 16 + col] << (28 - i * 4));
	}
	return des_permute32(out, P, 32);
}
static void des_block(const uint8_t *in, uint8_t *out, const uint8_t *key, bool dec)
{
	uint64_t sk[16];
	des_subkeys(key, sk);
	uint64_t blk = 0; int i;
	for (i = 0; i < 8; i++) blk |= ((uint64_t)in[i] << (56 - i * 8));
	blk = des_permute64(blk, IP, 64);
	uint32_t l = (uint32_t)(blk >> 32), r = (uint32_t)(blk & 0xFFFFFFFF);
	for (i = 0; i < 16; i++)
	{
		uint32_t tmp = r;
		r = l ^ des_f(r, dec ? sk[15 - i] : sk[i]);
		l = tmp;
	}
	blk = ((uint64_t)r << 32) | l;
	blk = des_permute64(blk, FP, 64);
	for (i = 0; i < 8; i++) out[i] = (uint8_t)((blk >> (56 - i * 8)) & 0xFF);
	secure_zero(sk, sizeof(sk));
}

void crypt_init(void) { /* reserved */ }

void crypt_des_enc(const uint8_t *key8, const uint8_t *in8, uint8_t *out8)
{
	des_block(in8, out8, key8, false);
}
void crypt_des_dec(const uint8_t *key8, const uint8_t *in8, uint8_t *out8)
{
	des_block(in8, out8, key8, true);
}

void crypt_key_spread(const uint8_t *k, uint8_t *s)
{
	int i;
	s[0]  = k[0] & 0xfe;
	s[1]  = ((k[0]  << 7) | (k[1]  >> 1)) & 0xfe;
	s[2]  = ((k[1]  << 6) | (k[2]  >> 2)) & 0xfe;
	s[3]  = ((k[2]  << 5) | (k[3]  >> 3)) & 0xfe;
	s[4]  = ((k[3]  << 4) | (k[4]  >> 4)) & 0xfe;
	s[5]  = ((k[4]  << 3) | (k[5]  >> 5)) & 0xfe;
	s[6]  = ((k[5]  << 2) | (k[6]  >> 6)) & 0xfe;
	s[7]  =  k[6]  << 1;
	s[8]  = k[7] & 0xfe;
	s[9]  = ((k[7]  << 7) | (k[8]  >> 1)) & 0xfe;
	s[10] = ((k[8]  << 6) | (k[9]  >> 2)) & 0xfe;
	s[11] = ((k[9]  << 5) | (k[10] >> 3)) & 0xfe;
	s[12] = ((k[10] << 4) | (k[11] >> 4)) & 0xfe;
	s[13] = ((k[11] << 3) | (k[12] >> 5)) & 0xfe;
	s[14] = ((k[12] << 2) | (k[13] >> 6)) & 0xfe;
	s[15] =  k[13] << 1;
	for (i = 0; i < 16; i++)
	{
		int par = 0, j;
		for (j = 1; j < 8; j++) par ^= (s[i] >> j) & 1;
		s[i] = (s[i] & 0xFE) | (par ^ 1);
	}
}

void crypt_ede2_cbc(const uint8_t *k16, const uint8_t *iv,
                     const uint8_t *in, uint8_t *out,
                     size_t len, bool encrypt)
{
	uint8_t ivec[8], tmp[8];
	size_t i; int j;
	memcpy(ivec, iv, 8);
	if (encrypt)
	{
		for (i = 0; i < len; i += 8)
		{
			for (j = 0; j < 8; j++) out[i+j] = in[i+j] ^ ivec[j];
			crypt_des_enc(k16,     out+i, out+i);
			crypt_des_dec(k16+8,   out+i, out+i);
			crypt_des_enc(k16,     out+i, out+i);
			memcpy(ivec, out+i, 8);
		}
	}
	else
	{
		for (i = 0; i < len; i += 8)
		{
			memcpy(tmp, in+i, 8);
			crypt_des_dec(k16,   in+i, out+i);
			crypt_des_enc(k16+8, out+i, out+i);
			crypt_des_dec(k16,   out+i, out+i);
			for (j = 0; j < 8; j++) out[i+j] ^= ivec[j];
			memcpy(ivec, tmp, 8);
			secure_zero(tmp, 8);
		}
	}
	secure_zero(ivec, sizeof(ivec));
}

#define F(x,y,z) (((x)&(y))|((~x)&(z)))
#define G(x,y,z) (((x)&(z))|((y)&(~z)))
#define H(x,y,z) ((x)^(y)^(z))
#define I(x,y,z) ((y)^((x)|(~z)))
#define ROL(x,n) (((x)<<(n))|((x)>>(32-(n))))

static void md5_transform(uint32_t st[4], const uint8_t blk[64])
{
	static const uint32_t T[64] = {
		0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
		0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
		0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
		0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
		0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
		0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
		0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
		0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
	};
	uint32_t a=st[0], b=st[1], c=st[2], d=st[3], x[16]; int i;
	for (i = 0; i < 16; i++) x[i] = rd_le32(blk + i * 4);

#define OP(f,a,b,c,d,k,s,i2) a=b+ROL(a+f(b,c,d)+x[k]+T[i2],s)
	OP(F,a,b,c,d, 0, 7, 0); OP(F,d,a,b,c, 1,12, 1); OP(F,c,d,a,b, 2,17, 2); OP(F,b,c,d,a, 3,22, 3);
	OP(F,a,b,c,d, 4, 7, 4); OP(F,d,a,b,c, 5,12, 5); OP(F,c,d,a,b, 6,17, 6); OP(F,b,c,d,a, 7,22, 7);
	OP(F,a,b,c,d, 8, 7, 8); OP(F,d,a,b,c, 9,12, 9); OP(F,c,d,a,b,10,17,10); OP(F,b,c,d,a,11,22,11);
	OP(F,a,b,c,d,12, 7,12); OP(F,d,a,b,c,13,12,13); OP(F,c,d,a,b,14,17,14); OP(F,b,c,d,a,15,22,15);
	OP(G,a,b,c,d, 1, 5,16); OP(G,d,a,b,c, 6, 9,17); OP(G,c,d,a,b,11,14,18); OP(G,b,c,d,a, 0,20,19);
	OP(G,a,b,c,d, 5, 5,20); OP(G,d,a,b,c,10, 9,21); OP(G,c,d,a,b,15,14,22); OP(G,b,c,d,a, 4,20,23);
	OP(G,a,b,c,d, 9, 5,24); OP(G,d,a,b,c,14, 9,25); OP(G,c,d,a,b, 3,14,26); OP(G,b,c,d,a, 8,20,27);
	OP(G,a,b,c,d,13, 5,28); OP(G,d,a,b,c, 2, 9,29); OP(G,c,d,a,b, 7,14,30); OP(G,b,c,d,a,12,20,31);
	OP(H,a,b,c,d, 5, 4,32); OP(H,d,a,b,c, 8,11,33); OP(H,c,d,a,b,11,16,34); OP(H,b,c,d,a,14,23,35);
	OP(H,a,b,c,d, 1, 4,36); OP(H,d,a,b,c, 4,11,37); OP(H,c,d,a,b, 7,16,38); OP(H,b,c,d,a,10,23,39);
	OP(H,a,b,c,d,13, 4,40); OP(H,d,a,b,c, 0,11,41); OP(H,c,d,a,b, 3,16,42); OP(H,b,c,d,a, 6,23,43);
	OP(H,a,b,c,d, 9, 4,44); OP(H,d,a,b,c,12,11,45); OP(H,c,d,a,b,15,16,46); OP(H,b,c,d,a, 2,23,47);
	OP(I,a,b,c,d, 0, 6,48); OP(I,d,a,b,c, 7,10,49); OP(I,c,d,a,b,14,15,50); OP(I,b,c,d,a, 5,21,51);
	OP(I,a,b,c,d,12, 6,52); OP(I,d,a,b,c, 3,10,53); OP(I,c,d,a,b,10,15,54); OP(I,b,c,d,a, 1,21,55);
	OP(I,a,b,c,d, 8, 6,56); OP(I,d,a,b,c,15,10,57); OP(I,c,d,a,b, 6,15,58); OP(I,b,c,d,a,13,21,59);
	OP(I,a,b,c,d, 4, 6,60); OP(I,d,a,b,c,11,10,61); OP(I,c,d,a,b, 2,15,62); OP(I,b,c,d,a, 9,21,63);
#undef OP
	st[0]+=a; st[1]+=b; st[2]+=c; st[3]+=d;
}
#undef F
#undef G
#undef H
#undef I
#undef ROL

void crypt_md5_hash(const uint8_t *data, size_t len, uint8_t out[16])
{
	uint32_t st[4] = {0x67452301,0xefcdab89,0x98badcfe,0x10325476};
	uint8_t  buf[64];
	size_t   i = 0, rem;
	int      j;
	while (i + 64 <= len) { md5_transform(st, data + i); i += 64; }
	rem = len - i;
	memcpy(buf, data + i, rem);
	buf[rem] = 0x80;
	memset(buf + rem + 1, 0, 64 - rem - 1);
	if (rem >= 56) { md5_transform(st, buf); memset(buf, 0, 64); }
	uint64_t bits = (uint64_t)len * 8;
	for (j = 0; j < 8; j++) buf[56 + j] = (uint8_t)((bits >> (j * 8)) & 0xFF);
	md5_transform(st, buf);
	for (j = 0; j < 4; j++) wr_le32(out + j * 4, st[j]);
	secure_zero(buf, sizeof(buf));
	secure_zero(st,  sizeof(st));
}

/* MD5-crypt encode table */
static const char MD5B64[] =
	"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static const char MD5_MAGIC[] = "$1$";

static void to64(uint32_t v, int n, char *dst, int *pos)
{
	int i;
	for (i = 0; i < n; i++) dst[(*pos)++] = MD5B64[v & 0x3f], v >>= 6;
}

bool crypt_md5_crypt(const char *pw, const char *salt_in, char *out, size_t outsz)
{
	const char *sp = salt_in;
	const char *ep;
	int         sl, i, pl;
	size_t      pw_len = strlen(pw);

	if (strncmp(sp, MD5_MAGIC, 3) == 0) sp += 3;
	ep = strchr(sp, '$');
	if (!ep) return false;
	sl = (int)(ep - sp);
	if (sl > 8) sl = 8;

	/* Build buffers on heap to avoid VLA */
	size_t  bsz = pw_len * 2 + 128;
	uint8_t *tmp  = (uint8_t *)malloc(bsz);
	uint8_t *atmp = (uint8_t *)malloc(pw_len * 2 + 32);
	uint8_t alt[16], fh[16];
	size_t  pos = 0, apos = 0;

	if (!tmp || !atmp) { free(tmp); free(atmp); return false; }

	/* ctx1 */
	memcpy(tmp + pos, pw, pw_len);     pos += pw_len;
	memcpy(tmp + pos, MD5_MAGIC, 3);   pos += 3;
	memcpy(tmp + pos, sp, sl);         pos += sl;

	/* alt hash */
	memcpy(atmp + apos, pw, pw_len);   apos += pw_len;
	memcpy(atmp + apos, sp, sl);       apos += sl;
	memcpy(atmp + apos, pw, pw_len);   apos += pw_len;
	crypt_md5_hash(atmp, apos, alt);

	for (pl = (int)pw_len; pl > 0; pl -= 16)
	{
		int take = pl > 16 ? 16 : pl;
		memcpy(tmp + pos, alt, take); pos += take;
	}
	secure_zero(alt, 16);

	for (i = (int)pw_len; i; i >>= 1)
		tmp[pos++] = (i & 1) ? 0 : pw[0];
	crypt_md5_hash(tmp, pos, fh);

	/* 1000-round stretch */
	for (i = 0; i < 1000; i++)
	{
		pos = 0;
		if (i & 1)  { memcpy(tmp+pos, pw, pw_len); pos += pw_len; }
		else        { memcpy(tmp+pos, fh, 16);      pos += 16;     }
		if (i % 3)  { memcpy(tmp+pos, sp, sl);      pos += sl;     }
		if (i % 7)  { memcpy(tmp+pos, pw, pw_len);  pos += pw_len; }
		if (i & 1)  { memcpy(tmp+pos, fh, 16);      pos += 16;     }
		else        { memcpy(tmp+pos, pw, pw_len);   pos += pw_len; }
		crypt_md5_hash(tmp, pos, fh);
	}

	/* Encode output */
	int opos = 0;
	char result[64] = {0};
	opos += snprintf(result + opos, sizeof(result) - opos, "%s", MD5_MAGIC);
	for (i = 0; i < sl; i++) result[opos++] = sp[i];
	result[opos++] = '$';

#define EM(a,b,c,n) do { uint32_t v=((uint32_t)fh[a]<<16)|((uint32_t)fh[b]<<8)|fh[c]; to64(v,n,result,&opos); } while(0)
	EM(0,6,12,4); EM(1,7,13,4); EM(2,8,14,4); EM(3,9,15,4); EM(4,10,5,4);
#undef EM
	to64(fh[11], 2, result, &opos);
	result[opos] = '\0';

	secure_zero(fh,  sizeof(fh));
	secure_zero(tmp, bsz);
	secure_zero(atmp, pw_len * 2 + 32);
	free(tmp); free(atmp);

	if ((size_t)opos >= outsz) return false;
	memcpy(out, result, opos + 1);
	return true;
}
