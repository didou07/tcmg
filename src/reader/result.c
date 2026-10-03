#include "result.h"
#include <string.h>

void reader_result_init(S_READER_RESULT *result)
{
    if (!result) return;
    memset(result, 0, sizeof(*result));
    result->status = -1;
    result->reader_index = -1;
    result->cache_hit = 0;
    result->source = TCMG_ECM_SOURCE_READER;
    result->failure = READER_FAILURE_NONE;
}
