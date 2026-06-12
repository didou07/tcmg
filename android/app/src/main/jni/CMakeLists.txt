cmake_minimum_required(VERSION 3.22.1)
project(tcmg C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

get_filename_component(REPO_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.."
    ABSOLUTE
)

if(NOT EXISTS "${REPO_ROOT}/src/main.c")
    message(FATAL_ERROR
        "Cannot find src/main.c in '${REPO_ROOT}'.\n"
        "Expected repo layout:\n"
        "  <repo>/src/main.c\n"
        "  <repo>/android/app/src/main/jni/CMakeLists.txt\n"
    )
endif()

add_compile_options(
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-implicit-fallthrough
    -Wno-overlength-strings
    -ffunction-sections
    -fdata-sections
    -fstack-protector-strong
    -fvisibility=hidden
)

add_definitions(
    -D_GNU_SOURCE
    -DANDROID
    -D__ANDROID__
    -Dmain=tcmg_server_main
    -DNO_DAEMON_SUPPORT=1
)

add_library(tcmg_core STATIC
    ${REPO_ROOT}/src/core/globals.c
    ${REPO_ROOT}/src/main.c
    ${REPO_ROOT}/src/client/client.c
    ${REPO_ROOT}/src/log/log.c
    ${REPO_ROOT}/src/config/config.c
    ${REPO_ROOT}/src/security/failban.c
    ${REPO_ROOT}/src/emu/emu.c
    ${REPO_ROOT}/src/srvid/srvid.c
    ${REPO_ROOT}/src/net/net.c
    ${REPO_ROOT}/src/platform/platform.c
    ${REPO_ROOT}/src/cache/cw_cache.c
    ${REPO_ROOT}/src/crypto/crypto.c
    ${REPO_ROOT}/src/crypto/sha1.c
    ${REPO_ROOT}/src/proto/cccam.c
    ${REPO_ROOT}/src/proto/newcamd.c
    ${REPO_ROOT}/webif/server.c
    ${REPO_ROOT}/webif/layout.c
    ${REPO_ROOT}/webif/stats.c
    ${REPO_ROOT}/webif/http/request.c
    ${REPO_ROOT}/webif/http/response.c
    ${REPO_ROOT}/webif/http/auth.c
    ${REPO_ROOT}/webif/pages/login.c
    ${REPO_ROOT}/webif/pages/status.c
    ${REPO_ROOT}/webif/pages/system.c
    ${REPO_ROOT}/webif/pages/users.c
    ${REPO_ROOT}/webif/pages/tvcas.c
    ${REPO_ROOT}/webif/pages/config.c
    ${REPO_ROOT}/webif/pages/files.c
    ${REPO_ROOT}/webif/pages/power.c
    ${REPO_ROOT}/webif/api/status.c
    ${REPO_ROOT}/webif/api/users.c
    ${REPO_ROOT}/webif/api/config.c
    ${REPO_ROOT}/webif/api/system.c
)

target_include_directories(tcmg_core PUBLIC
    ${REPO_ROOT}
    ${REPO_ROOT}/webif
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(tcmg_jni SHARED
    tcmg_jni_bridge.c
)

target_include_directories(tcmg_jni PRIVATE
    ${REPO_ROOT}
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
