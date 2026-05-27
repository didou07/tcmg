cmake_minimum_required(VERSION 3.22.1)
project(tcmg C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# =============================================================================
# Locate the unified C source tree.
#
# CMakeLists.txt lives at:
#   <repo>/android/app/src/main/jni/CMakeLists.txt
# Navigating up 5 levels reaches the repo root, then we go into src/:
#   jni/ → main/ → src/ → app/ → android/ → <repo root>
# =============================================================================
get_filename_component(REPO_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.."
    ABSOLUTE
)
set(TCMG_SRC "${REPO_ROOT}/src")

# Sanity check — fail early with a clear message if the path is wrong
if(NOT EXISTS "${TCMG_SRC}/tcmg.c")
    message(FATAL_ERROR
        "Cannot find tcmg.c in '${TCMG_SRC}'.\n"
        "Expected repo layout:\n"
        "  <repo>/src/tcmg.c\n"
        "  <repo>/android/app/src/main/jni/CMakeLists.txt\n"
    )
endif()

# =============================================================================
# Compiler flags
# =============================================================================
add_compile_options(
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-implicit-fallthrough
    -ffunction-sections
    -fdata-sections
    -fstack-protector-strong
    -fvisibility=hidden
)

# =============================================================================
# Preprocessor definitions
# =============================================================================
add_definitions(
    -D_GNU_SOURCE
    -DANDROID
    -D__ANDROID__
    # Rename main() → tcmg_server_main() — no conflict with Android entry point
    -Dmain=tcmg_server_main
    # Suppress fork/daemonize on Android
    -DNO_DAEMON_SUPPORT=1
)

# =============================================================================
# TCMG core — static library built from the unified src/ tree
# =============================================================================
add_library(tcmg_core STATIC
    ${TCMG_SRC}/tcmg.c
    ${TCMG_SRC}/tcmg-log.c
    ${TCMG_SRC}/tcmg-crypto.c
    ${TCMG_SRC}/tcmg-net.c
    ${TCMG_SRC}/tcmg-ban.c
    ${TCMG_SRC}/tcmg-conf.c
    ${TCMG_SRC}/tcmg-emu.c
    ${TCMG_SRC}/tcmg-srvid2.c
    ${TCMG_SRC}/tcmg-webif.c
    ${TCMG_SRC}/tcmg-webif-common.c
    ${TCMG_SRC}/tcmg-webif-layout.c
    ${TCMG_SRC}/tcmg-webif-pages.c
    ${TCMG_SRC}/tcmg-webif-tvcas.c
)

target_include_directories(tcmg_core PUBLIC
    ${TCMG_SRC}                    # tcmg-globals.h, tcmg-*.h
    ${CMAKE_CURRENT_SOURCE_DIR}    # tcmg_android_compat.h
)

# =============================================================================
# JNI bridge — shared library loaded by the Android runtime
# =============================================================================
add_library(tcmg_jni SHARED
    tcmg_jni_bridge.c
)

target_include_directories(tcmg_jni PRIVATE
    ${TCMG_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_options(tcmg_jni PRIVATE -Wl,--gc-sections)

target_link_libraries(tcmg_jni
    tcmg_core
    android
    log
    atomic
    m
)
