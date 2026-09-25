#include "cache/cw_cache.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    S_ACCOUNT one;
    S_ACCOUNT two;
    uint8_t md5[16];
    uint8_t cw[CW_LEN];
    uint8_t out[CW_LEN];
    int32_t groups[] = { 7 };

    memset(&one, 0, sizeof(one));
    memset(&two, 0, sizeof(two));
    for (int i = 0; i < 16; i++) md5[i] = (uint8_t)i;
    for (int i = 0; i < CW_LEN; i++) cw[i] = (uint8_t)(0xA0 + i);

    one.group = 7;
    one.ngroups = 0;
    two.group = 8;
    two.ngroups = 0;

    cw_cache_store_groups(md5, cw, groups, 1);

    memset(out, 0, sizeof(out));
    assert(cw_cache_lookup(md5, out, &one, true));
    assert(memcmp(out, cw, CW_LEN) == 0);

    memset(out, 0, sizeof(out));
    assert(!cw_cache_lookup(md5, out, &two, true));

    memset(out, 0, sizeof(out));
    assert(cw_cache_lookup(md5, out, &two, false));
    assert(memcmp(out, cw, CW_LEN) == 0);

    puts("cache_smoke: PASS");
    return 0;
}
