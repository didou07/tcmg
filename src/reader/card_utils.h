#ifndef TCMG_CARD_UTILS_H_
#define TCMG_CARD_UTILS_H_

#include <stddef.h>
#include <stdint.h>

int tcmg_parse_conax_cw(const uint8_t *rsp, size_t rsp_len, uint8_t cw[16], int *found_mask);

#endif
