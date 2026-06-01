#ifndef TCMG_SRVID_H_
#define TCMG_SRVID_H_

int         srvid_load(const char *path);
int  srvid_write_default(const char *path);
void        srvid_free(void);
const char *srvid_lookup(uint16_t caid, uint16_t sid);

#endif /* TCMG_SRVID_H_ */
