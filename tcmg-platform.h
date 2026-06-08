#ifndef TCMG_PLATFORM_H_
#define TCMG_PLATFORM_H_

#ifdef TCMG_OS_WINDOWS
#  define TCMG_PATH_SEP '\\'
#else
#  define TCMG_PATH_SEP '/'
#endif

void tcmg_mkdir(const char *path);

void tcmg_build_path(char *dst, size_t dstsz,
                     const char *dir, const char *file);

#ifdef TCMG_OS_WINDOWS
static inline int64_t tcmg_mono_ms(void)
{
    LARGE_INTEGER freq, cnt;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt);
    return (int64_t)(cnt.QuadPart * 1000LL / freq.QuadPart);
}
#else
static inline int64_t tcmg_mono_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)t.tv_sec * 1000LL + (int64_t)(t.tv_nsec / 1000000);
}
#endif

static inline int32_t tcmg_elapsed_ms(int64_t t0_ms)
{
    return (int32_t)(tcmg_mono_ms() - t0_ms);
}

void tcmg_setup_signals(volatile int32_t *running);

int tcmg_daemonise(void);

void tcmg_exec_restart(char **argv);

#endif