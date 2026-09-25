#ifndef TCMG_PROTO_REGISTRY_H_
#define TCMG_PROTO_REGISTRY_H_

#include <stddef.h>
#include <stdint.h>

typedef int32_t (*T_PROTO_START_FN)(void);
typedef void (*T_PROTO_STOP_FN)(void);

typedef struct {
    const char *name;
    T_PROTO_START_FN start;
    T_PROTO_STOP_FN stop;
} S_PROTO_MODULE;

size_t proto_module_count(void);
const S_PROTO_MODULE *proto_module_at(size_t index);
const S_PROTO_MODULE *proto_module_find(const char *name);
int32_t proto_start_all(void);
void    proto_stop_all(void);

#endif
