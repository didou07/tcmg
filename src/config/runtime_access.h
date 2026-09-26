#ifndef TCMG_CONFIG_RUNTIME_ACCESS_H_
#define TCMG_CONFIG_RUNTIME_ACCESS_H_

#include "../core/config_types.h"
#include <stdbool.h>
#include <stddef.h>

#define TCMG_RUNTIME_CONFIG_TEXT_MAX CFGVAL_LEN

typedef struct {
    bool enabled;
    int max_fails;
    int ban_secs;
    char allowlist[TCMG_RUNTIME_CONFIG_TEXT_MAX];
} S_CONFIG_FAILBAN_VIEW;

bool cfg_runtime_failban_snapshot(S_CONFIG_FAILBAN_VIEW *out);
int cfg_runtime_reader_snapshot(S_READER *out, size_t cap);
int cfg_runtime_reader_snapshot_indexed(S_READER *out, size_t cap);
bool cfg_runtime_reader_get(int index, S_READER *out);

#endif
