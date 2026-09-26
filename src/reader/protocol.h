#ifndef TCMG_READER_PROTOCOL_H_
#define TCMG_READER_PROTOCOL_H_

#include "ecm/request.h"
#include "core/constants.h"
#include "core/reader_types.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int index;
    const S_READER *reader;
    const S_ECM_REQUEST *request;
} S_READER_ECM_REQUEST;

typedef int32_t (*reader_protocol_do_ecm_fn)(const S_READER_ECM_REQUEST *req);
typedef void (*reader_protocol_shutdown_fn)(void);

typedef enum {
    READER_PROTOCOL_UNKNOWN = 0,
    READER_PROTOCOL_NETWORK,
    READER_PROTOCOL_CARD,
    READER_PROTOCOL_EMU
} E_READER_PROTOCOL_KIND;

typedef struct {
    const char *name;
    const char *alias;
    E_READER_PROTOCOL_KIND kind;
    reader_protocol_do_ecm_fn do_ecm;
    reader_protocol_shutdown_fn shutdown;
} S_READER_PROTOCOL;

const S_READER_PROTOCOL *reader_protocol_find(const char *name);
E_READER_PROTOCOL_KIND reader_protocol_kind(const char *name);
size_t reader_protocol_count(void);
const S_READER_PROTOCOL *reader_protocol_at(size_t index);

#endif
