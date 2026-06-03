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
if(NOT EXISTS "${TCMG_SRC}/main.c")
    message(FATAL_ERROR
        "Cannot find main.c in '${TCMG_SRC}'.\n"
        "Expected repo layout:\n"
        "  <repo>/src/main.c\n"
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
    -Dmain=tcmg_server_main
    -DNO_DAEMON_SUPPORT=1
)

if(DEFINED TCMG_VERSION)
    add_definitions(-DTCMG_VERSION="${TCMG_VERSION}")
endif()

# =============================================================================
# TCMG core — static library built from the unified src/ tree
# =============================================================================
add_library(tcmg_core STATIC
    ${TCMG_SRC}/main.c
    ${TCMG_SRC}/globals.c
    ${TCMG_SRC}/client.c
    ${TCMG_SRC}/core/log.c
    ${TCMG_SRC}/core/conf.c
    ${TCMG_SRC}/core/ban.c
    ${TCMG_SRC}/core/emu.c
    ${TCMG_SRC}/core/srvid.c
    ${TCMG_SRC}/crypto/crypto.c
    ${TCMG_SRC}/net/net.c
    ${TCMG_SRC}/platform/platform.c
    ${TCMG_SRC}/webif/webif.c
    ${TCMG_SRC}/webif/webif_api.c
    ${TCMG_SRC}/webif/webif_common.c
    ${TCMG_SRC}/webif/webif_layout.c
    ${TCMG_SRC}/webif/webif_page_login.c
    ${TCMG_SRC}/webif/webif_page_status.c
    ${TCMG_SRC}/webif/webif_page_system.c
    ${TCMG_SRC}/webif/webif_page_users.c
    ${TCMG_SRC}/webif/webif_tvcas.c
)

target_include_directories(tcmg_core PUBLIC
    ${TCMG_SRC}                    # tcmg_globals.h, tcmg_types.h, tcmg.h, etc.
    ${TCMG_SRC}/core               # log.h, conf.h, ban.h, emu.h, srvid.h
    ${TCMG_SRC}/crypto             # crypto.h
    ${TCMG_SRC}/net                # net.h
    ${TCMG_SRC}/platform           # platform.h
    ${TCMG_SRC}/webif              # webif.h, webif_internal.h
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
    ${TCMG_SRC}/core
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
