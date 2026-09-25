#include "reader/protocol.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const char *required[] = { "cccam", "newcamd", "cs378x", "emu", "pcsc", "internal" };
    size_t count = reader_protocol_count();

    if (count != sizeof(required) / sizeof(required[0])) {
        fprintf(stderr, "FAIL: protocol count=%zu\n", count);
        return 1;
    }

    for (size_t i = 0; i < count; i++) {
        const S_READER_PROTOCOL *p = reader_protocol_at(i);
        if (!p || strcasecmp(p->name, required[i]) != 0 || !p->do_ecm) {
            fprintf(stderr, "FAIL: protocol[%zu]\n", i);
            return 2;
        }
        if (strcmp(p->name, "newcamd") == 0 && !p->alias) {
            fprintf(stderr, "FAIL: newcamd alias missing\n");
            return 3;
        }
    }

    if (reader_protocol_find("CCCAm") != reader_protocol_find("cccam")) return 4;
    if (reader_protocol_find("MGcamd") != reader_protocol_find("newcamd")) return 5;
    if (reader_protocol_find("unknown") != NULL) return 6;
    if (reader_protocol_at(count) != NULL) return 7;

    puts("READER_REGISTRY: PASS");
    return 0;
}
