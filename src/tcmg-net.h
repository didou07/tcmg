
#ifndef TCMG_NET_H_
#define TCMG_NET_H_

/* Reliable read: returns -1 on error/EOF, len on success. */
int32_t net_recv_all(int fd, void *buf, int32_t len);

/* Reliable write: returns -1 on error, len on success. */
int32_t net_send_all(int fd, const void *buf, int32_t len);

/* Apply SO_RCVTIMEO / SO_SNDTIMEO */
void    net_set_timeout(int fd, int32_t seconds);

/* TCP_NODELAY + SO_KEEPALIVE + TCP_KEEPIDLe when available */
void    net_tune_socket(int fd);


/*
 * nc_init — send 14-byte random challenge, derive initial key pair.
 * Called immediately after accept() before any other message.
 */
void nc_init(S_CLIENT *cl, const uint8_t *des_key14, int32_t timeout);

/*
 * nc_recv — receive one framed message.
 * Returns payload length (>=0) or -1 on disconnect/error.
 * Decrypts in-place; fills sid, mid, pid, caid_hdr from inner header.
 */
int32_t nc_recv(S_CLIENT *cl, uint8_t *data,
                uint16_t *sid, uint16_t *mid,
                uint32_t *pid, uint16_t *caid_hdr);

/*
 * nc_send — encrypt and send a framed reply.
 * data[0] is the command byte; dlen includes cmd + len bytes.
 */
int32_t nc_send(S_CLIENT *cl, const uint8_t *data, int32_t dlen,
                uint16_t sid, uint16_t mid, uint32_t pid);

/* MSG_ADDCARD (0xD3) — used for mgcamd multi-CAID announce */
int32_t nc_send_addcard(S_CLIENT *cl, uint16_t caid,
                         uint32_t provid, uint16_t mid);

/* MSG_GET_VERSION (0xD6) — version string reply */
int32_t nc_send_version(S_CLIENT *cl, uint16_t mid);

/* Big-endian helpers — portable, no unaligned access */
static inline uint16_t be16(const uint8_t *p)
{
	return (uint16_t)((uint16_t)p[0] << 8 | p[1]);
}
static inline uint32_t be32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
	       (uint32_t)p[2] << 8  | (uint32_t)p[3];
}
static inline void wr_be16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)(v >> 8);
	p[1] = (uint8_t)(v);
}
static inline void wr_be32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)(v);
}
static inline uint32_t rd_le32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
	       (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static inline void wr_le32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v);       p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* XOR checksum used by Newcamd framing */
static inline uint8_t nc_xor(const uint8_t *d, int32_t n)
{
	uint8_t cs = 0;
	int32_t i;
	for (i = 0; i < n; i++) cs ^= d[i];
	return cs;
}

/* CSPRNG — wraps getrandom(2) / /dev/urandom */
bool    csprng(uint8_t *buf, size_t len);

/* Constant-time memory compare (timing-safe) */
bool    ct_memeq(const uint8_t *a, const uint8_t *b, size_t n);

/* Timing-safe string compare for fixed-length MD5-crypt hashes */
bool    ct_streq(const char *a, const char *b);

/* Wipe memory — not optimised away by compiler */
void    secure_zero(void *ptr, size_t len);

#endif /* TCMG_NET_H_ */
