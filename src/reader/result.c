#include "result.h"
#include <string.h>

void reader_result_init(S_READER_RESULT *result)
{
    if (!result) return;
    memset(result, 0, sizeof(*result));
    result->status = -1;
    result->reader_index = -1;
    result->cache_hit = 0;
    result->failure = READER_FAILURE_NONE;
}
