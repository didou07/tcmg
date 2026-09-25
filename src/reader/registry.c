#define MODULE_LOG_PREFIX "reader-registry"
#include "protocol.h"
#include "proto-adapters.h"
#include "protocols/cccam.h"
#include "protocols/cs378x.h"
#include "protocols/newcamd.h"
#include <string.h>

typedef struct {
    const S_READER_PROTOCOL *protocol;
} S_READER_PROTOCOL_DEF;

static const S_READER_PROTOCOL s_protocols[] = {
    { "cccam",   NULL,     reader_cccam_do_ecm,   cccam_reader_shutdown   },
    { "newcamd", "mgcamd", reader_newcamd_do_ecm, newcamd_reader_shutdown },
    { "cs378x",  NULL,     reader_cs378x_do_ecm,  cs378x_reader_shutdown  },
    { "emu",     NULL,     reader_emu_do_ecm,     NULL                    },
    { "pcsc",    NULL,     reader_pcsc_do_ecm,    NULL                    },
    { "internal",NULL,     reader_internal_do_ecm, NULL                  },
};

static size_t protocol_table_size(void)
{
    return sizeof(s_protocols) / sizeof(s_protocols[0]);
}

const S_READER_PROTOCOL *reader_protocol_find(const char *name)
{
    if (!name || !*name) return NULL;
    for (size_t i = 0; i < protocol_table_size(); i++) {
        if (strcasecmp(name, s_protocols[i].name) == 0) return &s_protocols[i];
        if (s_protocols[i].alias && strcasecmp(name, s_protocols[i].alias) == 0)
            return &s_protocols[i];
    }
    return NULL;
}

size_t reader_protocol_count(void)
{
    return protocol_table_size();
}

const S_READER_PROTOCOL *reader_protocol_at(size_t index)
{
    if (index >= protocol_table_size()) return NULL;
    return &s_protocols[index];
}
