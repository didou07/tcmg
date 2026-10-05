#include "card_utils.h"
#include <string.h>

int tcmg_parse_conax_cw(const uint8_t *rsp, size_t rsp_len, uint8_t cw[16], int *found_mask)
{
    if (!rsp || rsp_len < 2 || !cw || !found_mask) return -1;
    *found_mask = 0;
    if (rsp_len >= 3 && rsp[0] == 0x81 && ((rsp[2] >> 5) == 2)) return -2;
    const size_t data_len = rsp_len - 2u;
    size_t p = 0;
    while (p + 2u <= data_len) {
        const uint8_t tag = rsp[p];
        const uint8_t len = rsp[p + 1];
        const size_t end = p + 2u + len;
        if (end > data_len) break;
        if (tag == 0x25 && len >= 0x0D) {
            const uint8_t n = rsp[p + 4];
            if (n < 2 && p + 15u <= data_len) {
                memcpy(cw + ((size_t)n << 3), rsp + p + 7, 8);
                *found_mask |= 1 << n;
            }
        }
        p = end;
    }
    return 0;
}
