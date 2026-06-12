#ifndef TCMG_CW_CACHE_H_
#define TCMG_CW_CACHE_H_

bool cw_cache_lookup(const uint8_t *ecm_md5, uint8_t *cw_out);
void cw_cache_store(const uint8_t *ecm_md5, const uint8_t *cw);

#endif
