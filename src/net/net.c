#define MODULE_LOG_PREFIX "net"
#include "../../globals.h"

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
#ifdef TCMG_OS_WINDOWS
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
	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, SO_CAST(&one), sizeof(one)) < 0) {
		tcmg_log_dbg(D_WIRE, "setsockopt(TCP_NODELAY) failed");
	}
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

	payload_len = total_len - 8;

	tcmg_dump_dbg(D_WIRE, buf, (int32_t)(total_len),
	              "%s [newcamd/mgcamd] recv raw encrypted", cl->ip);

	memcpy(iv, buf + payload_len, 8);
	tcmg_dump_dbg(D_NEWCAMD, iv, 8, "%s [newcamd/mgcamd] recv IV", cl->ip);

	memcpy(key16,     cl->key1, 8);
	memcpy(key16 + 8, cl->key2, 8);
	crypt_ede2_cbc(key16, iv, buf, buf, payload_len, false);
	secure_zero(key16, sizeof(key16));
	secure_zero(iv,    sizeof(iv));

	if (nc_xor(buf, payload_len)) return -1;

	*mid      = be16(buf);
	*sid      = be16(buf + 2);
	*caid_hdr = be16(buf + 4);
	*pid      = ((uint32_t)buf[6] << 16) |
	            ((uint32_t)buf[7] <<  8) |
	             (uint32_t)buf[8];

	tcmg_dump_dbg(D_NEWCAMD, buf, (int32_t)payload_len,
	              "%s [newcamd/mgcamd] recv payload mid=%04X sid=%04X caid=%04X",
	              cl->ip, *mid, *sid, *caid_hdr);

	if (payload_len < (uint16_t)(NC_HDR_LEN + 5)) return -1;

	rlen = (((buf[3 + NC_HDR_LEN] << 8) | buf[4 + NC_HDR_LEN]) & 0x0FFF) + 3;
	if (rlen + 2 + NC_HDR_LEN > (uint32_t)payload_len) return -1;

	memcpy(data, buf + 2 + NC_HDR_LEN, rlen);

	tcmg_dump_dbg(D_NEWCAMD, data, (int32_t)rlen,
	              "%s [newcamd/mgcamd] recv cmd=0x%02X", cl->ip, data[0]);

	return (int32_t)rlen;
}

static int32_t nc_finalize_send(S_CLIENT *cl, uint32_t blen)
{
	uint8_t *buf = cl->send_buf;
	uint8_t  pad[8], iv[8], key16[16];
	uint32_t plen;

	plen = (8 - ((blen - 1) % 8)) % 8;
	csprng(pad, plen);
	memcpy(buf + blen, pad, plen);
	blen += plen;

	buf[blen] = nc_xor(buf + 2, blen - 2);
	blen++;

	csprng(iv, 8);
	memcpy(buf + blen, iv, 8);

	memcpy(key16,     cl->key1, 8);
	memcpy(key16 + 8, cl->key2, 8);
	crypt_ede2_cbc(key16, iv, buf + 2, buf + 2, blen - 2, true);
	secure_zero(key16, sizeof(key16));
	secure_zero(iv,    sizeof(iv));

	blen += 8;
	wr_be16(buf, (uint16_t)(blen - 2));
	tcmg_dump_dbg(D_WIRE, buf, (int32_t)blen,
	              "%s [newcamd/mgcamd] send raw encrypted", cl->ip);
	return net_send_all(cl->fd, buf, (int32_t)blen);
}

int32_t nc_send(S_CLIENT *cl, const uint8_t *data, int32_t dlen,
                uint16_t sid, uint16_t mid, uint32_t pid)
{
	uint8_t *buf = cl->send_buf;
	uint32_t blen;
	uint16_t caid = cl->account ? cl->account->caid : cl->caid;

	memset(buf + 2, 0, NC_HDR_LEN + 4);

	wr_be16(buf + 2, mid);
	wr_be16(buf + 4, sid);
	buf[6]  = (uint8_t)(caid  >> 8);
	buf[7]  = (uint8_t)(caid  & 0xFF);
	buf[8]  = (uint8_t)(pid   >> 16);
	buf[9]  = (uint8_t)(pid   >>  8);
	buf[10] = (uint8_t)(pid);
	buf[11] = 0x00;

	memcpy(buf + 12, data, dlen);
	buf[13] = (data[1] & 0xF0) | (((dlen - 3) >> 8) & 0x0F);
	buf[14] = (dlen - 3) & 0xFF;

	blen = (uint32_t)dlen + 12;

	tcmg_dump_dbg(D_NEWCAMD, data, dlen,
	              "%s [newcamd/mgcamd] send cmd=0x%02X sid=%04X mid=%04X",
	              cl->ip, data[0], sid, mid);

	return nc_finalize_send(cl, blen);
}

int32_t nc_send_addcard(S_CLIENT *cl, uint16_t caid,
                         uint32_t provid, uint16_t mid)
{
	uint8_t *buf = cl->send_buf;
	static const uint8_t payload[3] = { MSG_ADDCARD, 0x00, 0x00 };

	memset(buf + 2, 0, 12);
	memcpy(buf + 12, payload, 3);
	wr_be16(buf + 2, mid);
	buf[6]  = (uint8_t)(caid   >> 8);
	buf[7]  = (uint8_t)(caid   & 0xFF);
	buf[8]  = (uint8_t)(provid >> 16);
	buf[9]  = (uint8_t)(provid >>  8);
	buf[10] = (uint8_t)(provid & 0xFF);
	buf[11] = 0x00;

	return nc_finalize_send(cl, 15);
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
