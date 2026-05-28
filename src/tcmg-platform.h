#ifndef TCMG_PLATFORM_H_
#define TCMG_PLATFORM_H_

/*
 * tcmg-platform.h — single platform-abstraction layer (OSCam philosophy)
 *
 * Rule: every #ifdef TCMG_OS_* lives here or in tcmg-globals.h.
 * No translation unit outside this file should need an OS guard for
 * time, paths, directories, restarts, or socket timeouts.
 *
 * Included at the *bottom* of tcmg-globals.h so the string helpers
 * (tcmg_strlcpy / tcmg_strlcat) are available for tcmg_build_path().
 */

/* ════════════════════════════════════════════════════════════════════
 * 1. PATH SEPARATOR & DIRECTORY CREATION
 * ════════════════════════════════════════════════════════════════════*/

#ifdef TCMG_OS_WINDOWS
#  define TCMG_PATH_SEP '\\'
#else
#  define TCMG_PATH_SEP '/'
#endif

/* Create a directory; silently succeeds if it already exists. */
static inline void tcmg_mkdir(const char *path)
{
#ifdef TCMG_OS_WINDOWS
	CreateDirectoryA(path, NULL);   /* ERROR_ALREADY_EXISTS → ignored */
#else
	mkdir(path, 0755);              /* EEXIST → ignored                */
#endif
}

/*
 * Build a dir/filename path into dst.
 * Normalises the trailing separator so exactly one is inserted,
 * then flips all '/' to '\' on Windows.
 */
static inline void tcmg_build_path(char *dst, size_t dstsz,
                                   const char *dir, const char *file)
{
	tcmg_strlcpy(dst, dir, dstsz);
	size_t dlen = strlen(dst);

	/* Strip any trailing separators */
	while (dlen > 0 && (dst[dlen-1] == '/' || dst[dlen-1] == '\\'))
		dst[--dlen] = '\0';

	/* Append separator + filename */
	if (dlen + 2 + strlen(file) < dstsz)
	{
		dst[dlen]   = TCMG_PATH_SEP;
		dst[dlen+1] = '\0';
		tcmg_strlcat(dst, file, dstsz);
	}

#ifdef TCMG_OS_WINDOWS
	/* Flip all '/' to '\' for Windows-native paths */
	for (char *p = dst; *p; p++)
		if (*p == '/') *p = '\\';
#endif
}

/* ════════════════════════════════════════════════════════════════════
 * 2. MONOTONIC TIME
 *
 *   tcmg_mono_ms()       → int64_t   absolute monotonic milliseconds
 *   tcmg_elapsed_ms(t0)  → int32_t   ms since t0 was captured
 *
 * All callers store int64_t t0 = tcmg_mono_ms() and pass it to
 * tcmg_elapsed_ms().  The struct timespec intermediary and the
 * per-platform now_mono() shim are gone.
 * ════════════════════════════════════════════════════════════════════*/

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

/* ════════════════════════════════════════════════════════════════════
 * 3. SIGNAL / CONSOLE HANDLER SETUP
 *
 * tcmg_setup_signals(g_running_ptr) installs SIGINT/SIGTERM (POSIX)
 * or SetConsoleCtrlHandler (Windows) so main() stays OS-agnostic.
 * ════════════════════════════════════════════════════════════════════*/

#ifdef TCMG_OS_WINDOWS

static volatile int32_t *s_running_ptr = NULL;

static BOOL WINAPI _tcmg_console_handler(DWORD event)
{
	if ((event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT ||
	     event == CTRL_CLOSE_EVENT) && s_running_ptr)
	{
		*s_running_ptr = 0;
		return TRUE;
	}
	return FALSE;
}

static inline void tcmg_setup_signals(volatile int32_t *running)
{
	s_running_ptr = running;
	SetConsoleCtrlHandler(_tcmg_console_handler, TRUE);
}

#else /* POSIX */

static volatile int32_t *s_running_ptr = NULL;

static void _tcmg_sig_handler(int sig)
{
	(void)sig;
	if (s_running_ptr) *s_running_ptr = 0;
}

static inline void tcmg_setup_signals(volatile int32_t *running)
{
	struct sigaction sa;
	s_running_ptr = running;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = _tcmg_sig_handler;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT,  &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	signal(SIGPIPE, SIG_IGN);
}

#endif /* signal setup */

/* ════════════════════════════════════════════════════════════════════
 * 4. DAEMONISE
 *
 * tcmg_daemonise() — forks, detaches, redirects stdio to /dev/null.
 * On Windows (where fork() is unavailable) this is a no-op.
 * Returns 0 in the child (or on Windows), -1 on fork error.
 * ════════════════════════════════════════════════════════════════════*/
static inline int tcmg_daemonise(void)
{
#if defined(TCMG_OS_WINDOWS) || defined(NO_DAEMON_SUPPORT)
	return 0;
#else
	pid_t pid = fork();
	if (pid < 0) { perror("fork"); return -1; }
	if (pid > 0) _exit(0);   /* parent exits cleanly */
	setsid();
	int devnull = open("/dev/null", O_RDWR);
	if (devnull >= 0)
	{
		dup2(devnull, STDIN_FILENO);
		dup2(devnull, STDOUT_FILENO);
		dup2(devnull, STDERR_FILENO);
		if (devnull > 2) close(devnull);
	}
	return 0;
#endif
}

/* ════════════════════════════════════════════════════════════════════
 * 5. PROCESS RESTART (re-exec with same argv)
 *
 * tcmg_exec_restart(argv) — replace current process image (POSIX)
 * or spawn a new one and exit (Windows).
 * ════════════════════════════════════════════════════════════════════*/
static inline void tcmg_exec_restart(char **argv)
{
#ifdef TCMG_OS_WINDOWS
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	char cmdline[4096];
	ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	cmdline[0] = '\0';
	for (int i = 0; argv[i]; i++)
	{
		if (i > 0) tcmg_strlcat(cmdline, " ", sizeof(cmdline));
		tcmg_strlcat(cmdline, argv[i], sizeof(cmdline));
	}
	if (CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0,
	                   NULL, NULL, &si, &pi))
	{
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
#else
	execv(argv[0], argv);
	perror("execv restart failed");   /* only reached on error */
#endif
}

#endif /* TCMG_PLATFORM_H_ */
