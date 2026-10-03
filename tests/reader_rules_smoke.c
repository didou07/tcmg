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
    r.ecm_whitelist = 0x37;
    if (reader_rule_check(&r, &a, 0x0100, 0x0001, 0, 55) != READER_RULE_ALLOW) return 1;
    if (reader_rule_check(&r, &a, 0x0100, 0x0001, 0, 54) != READER_RULE_ECM_WHITELIST) return 2;
    if (reader_rule_check(&r, &a, 0x0100, 0x0001, 0, 56) != READER_RULE_ECM_WHITELIST) return 3;
    r.ecm_whitelist = 0;
    r.nidents = 1;
    r.idents[0].caid = 0x0B00;
    r.idents[0].provid = 0x000001;
    if (reader_rule_check(&r, &a, 0x0500, 0x0001, 0x000001, 55) != READER_RULE_IDENT) return 4;
    if (reader_rule_check(&r, &a, 0x0B00, 0x0001, 0x000002, 55) != READER_RULE_IDENT) return 5;
    if (reader_rule_check(&r, &a, 0x0B00, 0x0001, 0x000001, 55) != READER_RULE_ALLOW) return 6;
    r.nidents = 0;
    r.ncaids = 0;
    if (reader_rule_check(&r, &a, 0x0500, 0x0001, 0, 55) != READER_RULE_ALLOW) return 7;
    r.groups[0] = 2;
    r.ngroups = 1;
    r.ecm_whitelist = 0x37;
    if (reader_rule_check(&r, &a, 0x0500, 0x0001, 0, 54) != READER_RULE_ECM_WHITELIST) return 8;
    if (reader_rule_check(&r, &a, 0x0500, 0x0001, 0, 55) != READER_RULE_GROUP) return 9;
    return 0;
}
