#ifndef TCMG_EMU_H_
#define TCMG_EMU_H_

void    emu_init(void);
int32_t emu_process(uint16_t caid, uint16_t sid,
                    const uint8_t *ecm, int32_t ecm_len,
                    uint8_t *cw, const S_ECM_CTX *ctx);

#endif
