/**
 * tcmg_android_compat.h
 * Compatibility shims for building tcmg on Android
 */
#pragma once

#ifdef __ANDROID__

/* Rename main() so it doesn't conflict with Android's entry point */
#define main tcmg_server_main

/* Android has fork() but we don't want daemonize behaviour */
#ifndef NO_DAEMON_SUPPORT
#  define NO_DAEMON_SUPPORT 1
#endif

#endif /* __ANDROID__ */
