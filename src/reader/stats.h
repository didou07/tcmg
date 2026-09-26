#ifndef TCMG_READER_STATS_H_
#define TCMG_READER_STATS_H_

#include <stdbool.h>
#include <stdint.h>

#include "core/constants.h"

typedef struct {
    int64_t cw_ok;
    int64_t cw_nok;
    int active;
} S_READER_STATS_SNAPSHOT;

void reader_stats_record(int index, bool success);
void reader_stats_snapshot(int index, S_READER_STATS_SNAPSHOT *out);
void reader_stats_reset(int index);

#endif
