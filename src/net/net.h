#ifndef TCMG_NET_H_
#define TCMG_NET_H_

int32_t net_recv_all(int fd, void *buf, int32_t len);
int32_t net_send_all(int fd, const void *buf, int32_t len);
void    net_set_timeout(int fd, int32_t seconds);
void    net_tune_socket(int fd);

void    nc_init(S_CLIENT *cl, const uint8_t *des_key14, int32_t timeout);
int32_t nc_recv(S_CLIENT *cl, uint8_t *data, uint16_t *sid, uint16_t *mid,
                uint32_t *pid, uint16_t *caid_hdr);
int32_t nc_send(S_CLIENT *cl, const uint8_t *data, int32_t dlen,
                uint16_t sid, uint16_t mid, uint32_t pid);
int32_t nc_send_addcard(S_CLIENT *cl, uint16_t caid, uint32_t provid, uint16_t mid);
int32_t nc_send_version(S_CLIENT *cl, uint16_t mid);

/* Big-endian helpers */
static inline uint16_t be16(const uint8_t *p)
    { return (uint16_t)((uint16_t)p[0]<<8|p[1]); }
static inline uint32_t be32(const uint8_t *p)
    { return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|(uint32_t)p[3]; }
static inline void wr_be16(uint8_t *p, uint16_t v)
    { p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }
static inline void wr_be32(uint8_t *p, uint32_t v)
    { p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v; }
static inline uint32_t rd_le32(const uint8_t *p)
    { return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static inline void wr_le32(uint8_t *p, uint32_t v)
    { p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24); }
static inline uint8_t nc_xor(const uint8_t *d, int32_t n)
    { uint8_t cs=0; for(int32_t i=0;i<n;i++) cs^=d[i]; return cs; }

#endif /* TCMG_NET_H_ */
