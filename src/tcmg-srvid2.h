#ifndef TCMG_SRVID2_H
#define TCMG_SRVID2_H

#include <stdint.h>

/* Load/reload tcmg.srvid2 — thread-safe (swaps internal table).
 * Returns number of entries loaded, or -1 on error.           */
int  srvid_load(const char *path);

/* Create a default tcmg.srvid2 with sample channel names.
 * Returns 1 on success, 0 on failure.                         */
int  srvid_write_default(const char *path);

/* Look up channel name by CAID + SID.
 * Returns pointer valid until next srvid_load(), or NULL.     */
const char *srvid_lookup(uint16_t caid, uint16_t sid);

/* Release all memory — call on shutdown. */
void srvid_free(void);

#endif /* TCMG_SRVID2_H */
