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

#endif
