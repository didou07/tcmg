#ifndef TCMG_PLATFORM_H_
#define TCMG_PLATFORM_H_

#ifdef TCMG_OS_WINDOWS
#  define TCMG_PATH_SEP '\\'
#else
#  define TCMG_PATH_SEP '/'
#endif

/* Create a directory; silently succeeds if it already exists. */
void tcmg_mkdir(const char *path);

/*
 * Build "dir/file" (or "dir\file" on Windows) into dst[dstsz].
 * Normalises trailing separators; on Windows also converts '/' → '\'.
 */
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

/*
 * Install SIGINT + SIGTERM handlers (POSIX) or SetConsoleCtrlHandler
 * (Windows) that set *running = 0 on receipt.
 * Must be called exactly once from main().
 */
void tcmg_setup_signals(volatile int32_t *running);

/*
 * Fork, detach from the terminal, redirect stdio to /dev/null.
 * Returns 0 in the child (or on Windows, where this is a no-op).
 * Returns -1 on fork error; the process should exit.
 */
int tcmg_daemonise(void);

/*
 * Replace the current process image with a fresh copy (POSIX execv),
 * or spawn a new process and exit (Windows CreateProcess).
 * argv must be the original argv[] received by main().
 */
void tcmg_exec_restart(char **argv);

#endif /* TCMG_PLATFORM_H_ */