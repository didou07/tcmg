#ifndef TCMG_UTILS_H_
#define TCMG_UTILS_H_

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

static inline uint16_t be16(const uint8_t *p)
    { return (uint16_t)((uint16_t)p[0]<<8|p[1]); }
static inline uint32_t be32(const uint8_t *p)
    { return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|(uint32_t)p[3]; }
static inline void wr_be16(uint8_t *p, uint16_t v)
    { p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }
static inline void wr_be32(uint8_t *p, uint32_t v)
    { p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v; }
static inline uint32_t rd_le32(const uint8_t *p)
    { return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static inline void wr_le32(uint8_t *p, uint32_t v)
    { p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24); }
static inline uint8_t nc_xor(const uint8_t *d, int32_t n)
    { uint8_t cs=0; for(int32_t i=0;i<n;i++) cs^=d[i]; return cs; }

#endif
