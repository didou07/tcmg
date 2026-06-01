#ifndef TCMG_COMPAT_H_
#define TCMG_COMPAT_H_

#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__)
#  ifndef _POSIX_THREAD_SAFE_FUNCTIONS
#    define _POSIX_THREAD_SAFE_FUNCTIONS 200112L
#  endif
#else
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#endif

#if defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__) || defined(__MINGW64__)
#  define TCMG_OS_WINDOWS 1
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0601   /* Windows 7+ */
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  include <io.h>
   /* Map POSIX socket/IO names to Winsock equivalents */
#  define close(fd)    closesocket(fd)
#  define MSG_NOSIGNAL 0
#  define ssize_t      int
#  define socklen_t    int
#  if !defined(__MINGW32__) && !defined(__MINGW64__)
     /* MSVC-only shims */
     static inline struct tm *localtime_r(const time_t *t, struct tm *s)
         { localtime_s(s, t); return s; }
     static inline struct tm *gmtime_r(const time_t *t, struct tm *s)
         { gmtime_s(s, t); return s; }
     static inline size_t strnlen(const char *s, size_t n)
         { const char *p = (const char *)memchr(s, 0, n);
           return p ? (size_t)(p - s) : n; }
     static inline unsigned int sleep(unsigned int sec)
         { Sleep(sec * 1000u); return 0; }
#  else
     /* MinGW: sleep() via <unistd.h>; localtime_r via _POSIX_THREAD_SAFE_FUNCTIONS */
#    include <unistd.h>
#  endif
   static inline int  tcmg_winsock_init(void)
       { WSADATA w; return WSAStartup(MAKEWORD(2,2), &w); }
   static inline void tcmg_winsock_cleanup(void) { WSACleanup(); }

#else
#  define TCMG_OS_POSIX 1
#  include <unistd.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <sys/socket.h>
#  include <sys/time.h>
#  include <sys/select.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <poll.h>
#  ifdef __APPLE__
#    ifndef MSG_NOSIGNAL
#      define MSG_NOSIGNAL 0
#    endif
#  endif
#  define tcmg_winsock_init()    ((void)0)
#  define tcmg_winsock_cleanup() ((void)0)
#endif /* platform */

#include <pthread.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <signal.h>

#ifdef TCMG_OS_WINDOWS
#  define SO_CAST(p)   ((const char *)(p))
#  define RECV_CAST(p) ((char *)(p))
#else
#  define SO_CAST(p)   (p)
#  define RECV_CAST(p) (p)
#endif

#endif /* TCMG_COMPAT_H_ */
