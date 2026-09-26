#define MODULE_LOG_PREFIX "reader"
#include "protocol.h"
#include "proto-adapters.h"
#include "protocols/cccam.h"
#include "protocols/cs378x.h"
#include "protocols/newcamd.h"
#include "../serial/serial.h"
#include <string.h>

static const S_READER_PROTOCOL s_protocols[] = {
    { "cccam",    NULL,     READER_PROTOCOL_NETWORK, reader_cccam_do_ecm,    cccam_reader_shutdown    },
    { "newcamd",  "mgcamd", READER_PROTOCOL_NETWORK, reader_newcamd_do_ecm,  newcamd_reader_shutdown  },
    { "cs378x",   NULL,     READER_PROTOCOL_NETWORK, reader_cs378x_do_ecm,   cs378x_reader_shutdown   },
    { "emu",      NULL,     READER_PROTOCOL_EMU,     reader_emu_do_ecm,      NULL                     },
    { "pcsc",     NULL,     READER_PROTOCOL_CARD,    reader_pcsc_do_ecm,     NULL                     },
    { "internal", NULL,     READER_PROTOCOL_CARD,    reader_internal_do_ecm, NULL                     },
    { "serial",   NULL,     READER_PROTOCOL_CARD,    reader_serial_do_ecm,   NULL                     },
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

E_READER_PROTOCOL_KIND reader_protocol_kind(const char *name)
{
    const S_READER_PROTOCOL *p = reader_protocol_find(name);
    return p ? p->kind : READER_PROTOCOL_UNKNOWN;
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
