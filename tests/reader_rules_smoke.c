#include <string.h>
#include "../src/reader/rules.h"
#include "../src/core/account_types.h"
#include "../src/core/reader_types.h"

int main(void)
{
    S_ACCOUNT a;
    S_READER r;
    memset(&a, 0, sizeof(a));
    memset(&r, 0, sizeof(r));
    a.group = 1;
    r.in_use = 1;
    r.enabled = 1;
    r.ecm_whitelist = 64;
    if (reader_rule_check(&r, &a, 0x0100, 0x0001, 64) != READER_RULE_ALLOW) return 1;
    if (reader_rule_check(&r, &a, 0x0100, 0x0001, 65) != READER_RULE_ECM_WHITELIST) return 2;
    return 0;
}
