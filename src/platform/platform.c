#define MODULE_LOG_PREFIX "platform"
#include "tcmg.h"

void tcmg_mkdir(const char *path)
{
#ifdef TCMG_OS_WINDOWS
    CreateDirectoryA(path, NULL);   /* ERROR_ALREADY_EXISTS is silently ignored */
#else
    mkdir(path, 0755);              /* EEXIST is silently ignored */
#endif
}

void tcmg_build_path(char *dst, size_t dstsz,
                     const char *dir, const char *file)
{
    tcmg_strlcpy(dst, dir, dstsz);
    size_t dlen = strlen(dst);

    /* Strip any trailing separators */
    while (dlen > 0 && (dst[dlen - 1] == '/' || dst[dlen - 1] == '\\'))
        dst[--dlen] = '\0';

    /* Append exactly one separator + filename */
    if (dlen + 2 + strlen(file) < dstsz) {
        dst[dlen]     = TCMG_PATH_SEP;
        dst[dlen + 1] = '\0';
        tcmg_strlcat(dst, file, dstsz);
    }

#ifdef TCMG_OS_WINDOWS
    /* Normalise all '/' to '\' for Windows-native paths */
    for (char *p = dst; *p; p++)
        if (*p == '/') *p = '\\';
#endif
}

static volatile int32_t *s_running_ptr = NULL;

#ifdef TCMG_OS_WINDOWS

static BOOL WINAPI _console_handler(DWORD event)
{
    if ((event == CTRL_C_EVENT     ||
         event == CTRL_BREAK_EVENT ||
         event == CTRL_CLOSE_EVENT) && s_running_ptr)
    {
        *s_running_ptr = 0;
        return TRUE;
    }
    return FALSE;
}

void tcmg_setup_signals(volatile int32_t *running)
{
    s_running_ptr = running;
    SetConsoleCtrlHandler(_console_handler, TRUE);
}

#else /* POSIX */

static void _sig_handler(int sig)
{
    (void)sig;
    if (s_running_ptr) *s_running_ptr = 0;
}

void tcmg_setup_signals(volatile int32_t *running)
{
    struct sigaction sa;
    s_running_ptr  = running;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler  = _sig_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
}

#endif /* POSIX */

int tcmg_daemonise(void)
{
#if defined(TCMG_OS_WINDOWS) || defined(NO_DAEMON_SUPPORT)
    return 0;   /* no-op: Windows services use SCM, not fork() */
#else
    pid_t pid = fork();
    if (pid < 0)  { perror("fork"); return -1; }
    if (pid > 0)  _exit(0);   /* parent exits cleanly */

    setsid();

    int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull > STDERR_FILENO) close(devnull);
    }
    return 0;
#endif
}

void tcmg_exec_restart(char **argv)
{
#ifdef TCMG_OS_WINDOWS
    STARTUPINFOA        si;
    PROCESS_INFORMATION pi;
    char cmdline[4096];

    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    cmdline[0] = '\0';

    for (int i = 0; argv[i]; i++) {
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
    perror("execv restart failed");   /* reached only on error */
#endif
}