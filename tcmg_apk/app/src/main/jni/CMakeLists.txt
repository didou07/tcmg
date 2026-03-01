cmake_minimum_required(VERSION 3.22.1)
project(tcmg C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── Compiler flags ────────────────────────────────────────────────────────────
# Enable warnings; treat most as errors except the ones the legacy C code
# commonly triggers (unused-parameter, sign-compare, implicit-fallthrough).
add_compile_options(
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-implicit-fallthrough
    -ffunction-sections
    -fdata-sections
)

# ── Preprocessor definitions ──────────────────────────────────────────────────
add_definitions(
    -D_GNU_SOURCE
    -DANDROID
    -D__ANDROID__
    # Rename main() → tcmg_server_main() to avoid conflict with Android entry point
    -Dmain=tcmg_server_main
    # Suppress daemonize (fork) on Android
    -DNO_DAEMON_SUPPORT=1
)

# ── TCMG core (static) ────────────────────────────────────────────────────────
# Each source file listed exactly once.
add_library(tcmg_core STATIC
    tcmg/tcmg.c
    tcmg/tcmg-log.c
    tcmg/tcmg-crypto.c
    tcmg/tcmg-net.c
    tcmg/tcmg-ban.c
    tcmg/tcmg-conf.c
    tcmg/tcmg-emu.c
    tcmg/tcmg-srvid2.c
    tcmg/tcmg-webif.c
)

target_include_directories(tcmg_core PUBLIC tcmg/)

# ── JNI bridge (shared library loaded by Android) ─────────────────────────────
add_library(tcmg_jni SHARED
    tcmg_jni_bridge.c
)

target_include_directories(tcmg_jni PRIVATE tcmg/)

# Dead-code stripping at link time
target_link_options(tcmg_jni PRIVATE -Wl,--gc-sections)

target_link_libraries(tcmg_jni
    tcmg_core
    android
    log
    atomic
    m
)
