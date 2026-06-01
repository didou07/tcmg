#ifndef TCMG_HELPERS_H_
#define TCMG_HELPERS_H_

#include "tcmg_compat.h"

static inline size_t tcmg_strlcpy(char *dst, const char *src, size_t sz)
{
    size_t n = 0;
    if (sz > 0) {
        for (; n < sz - 1 && src[n]; n++) dst[n] = src[n];
        dst[n] = '\0';
    }
    return n;
}

static inline size_t tcmg_strlcat(char *dst, const char *src, size_t sz)
{
    size_t dlen = strnlen(dst, sz);
    if (dlen >= sz) return sz;
    return dlen + tcmg_strlcpy(dst + dlen, src, sz - dlen);
}

static inline void format_uptime(time_t s, char *out, size_t sz)
{
    int d  = (int)(s / 86400);
    int h  = (int)((s % 86400) / 3600);
    int m  = (int)((s % 3600) / 60);
    int sc = (int)(s % 60);
    if (d > 0) snprintf(out, sz, "%dd %02dh %02dm %02ds", d, h, m, sc);
    else        snprintf(out, sz, "%02dh %02dm %02ds", h, m, sc);
}

static inline void format_time(time_t t, char *out, size_t sz)
{
    if (!t) { tcmg_strlcpy(out, "never", sz); return; }
    struct tm tm_s;
    localtime_r(&t, &tm_s);
    strftime(out, sz, "%Y-%m-%d %H:%M", &tm_s);
}

#endif /* TCMG_HELPERS_H_ */
