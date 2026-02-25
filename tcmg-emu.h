
#ifndef TCMG_EMU_H_
#define TCMG_EMU_H_

/* One-time init: calls crypt_init(). */
void emu_init(void);

/*
 * emu_process — attempt to decode ECM and fill cw[CW_LEN].
 * Returns one of the EMU_* result codes.
 *
 * Caller must zero cw[] before calling.
 * cw[] is zeroed by this function on any failure path.
 */
int32_t emu_process(uint16_t caid, uint16_t sid,
                    const uint8_t *ecm, int32_t ecm_len,
                    uint8_t *cw, const S_ECM_CTX *ctx);

#endif /* TCMG_EMU_H_ */
