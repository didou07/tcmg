#ifndef TCMG_SRVID_H_
#define TCMG_SRVID_H_

#define SRVID_NAME_MAX 80

int         srvid_load(const char *path);
int         srvid_write_default(const char *path);
void        srvid_free(void);
const char *srvid_lookup(uint16_t caid, uint16_t sid);
void        srvid_lookup_copy(uint16_t caid, uint16_t sid, char *buf, size_t bufsz);

#endif
