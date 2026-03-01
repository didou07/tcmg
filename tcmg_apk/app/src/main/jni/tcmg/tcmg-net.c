
#define MODULE_LOG_PREFIX "net"
#include "tcmg-globals.h"

/* SO_CAST / RECV_CAST defined centrally in tcmg-globals.h */

int32_t net_recv_all(int fd, void *buf, int32_t len)
{
	uint8_t *p = (uint8_t *)buf;
	int32_t  total = 0;
	while (total < len)
	{
		ssize_t n = recv(fd, RECV_CAST(p + total), len - total, 0);
		if (n <= 0) return -1;
		total += (int32_t)n;
	}
	return total;
}

int32_t net_send_all(int fd, const void *buf, int32_t len)
{
	const uint8_t *p = (const uint8_t *)buf;
	int32_t total = 0;
	while (total < len)
	{
		ssize_t n = send(fd, SO_CAST(p + total), len - total, 0);
		if (n <= 0) return -1;
		total += (int32_t)n;
	}
	return total;
}

void net_set_timeout(int fd, int32_t seconds)
{
#if defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__) || defined(__MINGW64__)
	/* Winsock SO_RCVTIMEO / SO_SNDTIMEO take a DWORD millisecond value */
	DWORD ms = (DWORD)(seconds * 1000);
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, SO_CAST(&ms), sizeof(ms));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, SO_CAST(&ms), sizeof(ms));
#else
	struct timeval tv = { seconds, 0 };
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, SO_CAST(&tv), sizeof(tv));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, SO_CAST(&tv), sizeof(tv));
#endif
}

void net_tune_socket(int fd)
{
	int one = 1;
#ifdef TCP_NODELAY
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, SO_CAST(&one), sizeof(one));
#endif
#ifdef SO_KEEPALIVE
	setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, SO_CAST(&one), sizeof(one));
#endif
#ifdef TCP_KEEPIDLE
	{ int idle=60, intvl=10, cnt=3;
	  setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE,  SO_CAST(&idle),  sizeof(idle));
	  setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, SO_CAST(&intvl), sizeof(intvl));
	  setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT,   SO_CAST(&cnt),   sizeof(cnt));  }
#endif
	(void)one;
}

void nc_init(S_CLIENT *cl, const uint8_t *des_key14, int32_t timeout)
{
	uint8_t rnd[14], spread[16];

	net_set_timeout(cl->fd, timeout);
	net_tune_socket(cl->fd);

	csprng(rnd, 14);
	if (net_send_all(cl->fd, rnd, 14) != 14)
	{
		secure_zero(rnd, sizeof(rnd));
		return;
	}

	memcpy(cl->session_key, des_key14, 14);
	{
		int i;
		for (i = 0; i < 14; i++) rnd[i] ^= des_key14[i];
	}
	crypt_key_spread(rnd, spread);
	memcpy(cl->key1, spread,     8);
	memcpy(cl->key2, spread + 8, 8);

	secure_zero(rnd,    sizeof(rnd));
	secure_zero(spread, sizeof(spread));
}

int32_t nc_recv(S_CLIENT *cl, uint8_t *data,
                uint16_t *sid, uint16_t *mid,
                uint32_t *pid, uint16_t *caid_hdr)
{
	uint8_t  lenbuf[2];
	uint8_t *buf = cl->recv_buf;
	uint8_t  iv[8], key16[16];
	uint16_t total_len, payload_len;
	uint32_t rlen;

	if (net_recv_all(cl->fd, lenbuf, 2) != 2) return -1;
	total_len = be16(lenbuf);
	if (total_len == 0 || total_len > NC_MSG_MAX) return -1;
	if (net_recv_all(cl->fd, buf, total_len) != (int32_t)total_len) return -1;
	if (total_len < 8) return -1;

	tcmg_log_dbg(D_NET, "%s recv %u bytes (encrypted)", cl->ip, total_len + 2u);

	payload_len = total_len - 8;

	/* Decrypt */
	memcpy(iv, buf + payload_len, 8);
	memcpy(key16,     cl->key1, 8);
	memcpy(key16 + 8, cl->key2, 8);
	crypt_ede2_cbc(key16, iv, buf, buf, payload_len, false);
	secure_zero(key16, sizeof(key16));
	secure_zero(iv,    sizeof(iv));

	/* Verify XOR checksum */
	if (nc_xor(buf, payload_len)) return -1;

	/* Parse inner header */
	*mid      = be16(buf);
	*sid      = be16(buf + 2);
	*caid_hdr = be16(buf + 4);
	*pid      = ((uint32_t)buf[6] << 16) | ((uint32_t)buf[7] << 8) | buf[8];

	/* Need at least HDR + 5 more bytes to read rlen */
	if (payload_len < (uint16_t)(NC_HDR_LEN + 5)) return -1;

	rlen = (((buf[3 + NC_HDR_LEN] << 8) | buf[4 + NC_HDR_LEN]) & 0x0FFF) + 3;
	if (rlen + 2 + NC_HDR_LEN > (uint32_t)payload_len) return -1;

	memcpy(data, buf + 2 + NC_HDR_LEN, rlen);
	return (int32_t)rlen;
}

static int32_t nc_finalize_send(S_CLIENT *cl, uint32_t blen)
{
	uint8_t *buf = cl->send_buf;
	uint8_t  pad[8], iv[8], key16[16];
	uint32_t plen;

	/* Pad to 8-byte boundary */
	plen = (8 - ((blen - 1) % 8)) % 8;
	csprng(pad, plen);
	memcpy(buf + blen, pad, plen);
	blen += plen;

	/* XOR checksum byte */
	buf[blen] = nc_xor(buf + 2, blen - 2);
	blen++;

	/* Append IV */
	csprng(iv, 8);
	memcpy(buf + blen, iv, 8);

	/* Encrypt payload (skip 2-byte outer length prefix) */
	memcpy(key16,     cl->key1, 8);
	memcpy(key16 + 8, cl->key2, 8);
	crypt_ede2_cbc(key16, iv, buf + 2, buf + 2, blen - 2, true);
	secure_zero(key16, sizeof(key16));
	secure_zero(iv,    sizeof(iv));

	blen += 8;
	wr_be16(buf, (uint16_t)(blen - 2));
	tcmg_log_dbg(D_NET, "%s send %u bytes (encrypted)", cl->ip, blen);
	return net_send_all(cl->fd, buf, (int32_t)blen);
}

int32_t nc_send(S_CLIENT *cl, const uint8_t *data, int32_t dlen,
                uint16_t sid, uint16_t mid, uint32_t pid)
{
	uint8_t *buf = cl->send_buf;
	uint32_t blen;

	memset(buf + 2, 0, NC_HDR_LEN + 2);
	memcpy(buf + NC_HDR_LEN + 4, data, dlen);
	buf[NC_HDR_LEN + 5] = (data[1] & 0xF0) | (((dlen - 3) >> 8) & 0x0F);
	buf[NC_HDR_LEN + 6] = (dlen - 3) & 0xFF;

	wr_be16(buf + 2, mid);
	wr_be16(buf + 4, sid);
	buf[6] = (uint8_t)(pid >> 16);
	buf[7] = (uint8_t)(pid >> 8);
	buf[8] = (uint8_t)(pid);

	blen = (uint32_t)dlen + NC_HDR_LEN + 4;
	return nc_finalize_send(cl, blen);
}

int32_t nc_send_addcard(S_CLIENT *cl, uint16_t caid,
                         uint32_t provid, uint16_t mid)
{
	uint8_t *buf = cl->send_buf;
	static const uint8_t payload[3] = { MSG_ADDCARD, 0x00, 0x00 };

	memset(buf + 2, 0, NC_HDR_LEN + 2);
	memcpy(buf + NC_HDR_LEN + 4, payload, 3);
	wr_be16(buf + 2, mid);
	buf[6]  = (uint8_t)(caid >> 8);
	buf[7]  = (uint8_t)(caid & 0xFF);
	buf[8]  = (uint8_t)(provid >> 16);
	buf[9]  = (uint8_t)(provid >> 8);
	buf[10] = (uint8_t)(provid & 0xFF);

	return nc_finalize_send(cl, 3 + NC_HDR_LEN + 4);
}

int32_t nc_send_version(S_CLIENT *cl, uint16_t mid)
{
	static const char VER[] = "1.67";
	uint8_t buf[7];
	buf[0] = MSG_GET_VERSION;
	buf[1] = 0;
	buf[2] = (uint8_t)strlen(VER);
	memcpy(buf + 3, VER, strlen(VER));
	return nc_send(cl, buf, (int32_t)(3 + strlen(VER)), 0, mid, 0);
}
