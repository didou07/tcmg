cmake_minimum_required(VERSION 3.22.1)
project(tcmg C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

get_filename_component(REPO_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.."
    ABSOLUTE
)

if(NOT EXISTS "${REPO_ROOT}/tcmg-main.c")
    message(FATAL_ERROR
        "Cannot find tcmg-main.c in '${REPO_ROOT}'.\n"
        "Expected repo layout:\n"
        "  <repo>/tcmg-main.c\n"
        "  <repo>/android/app/src/main/jni/CMakeLists.txt\n"
    )
endif()

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

add_definitions(
    -D_GNU_SOURCE
    -DANDROID
    -D__ANDROID__
    -Dmain=tcmg_server_main
    -DNO_DAEMON_SUPPORT=1
)

add_library(tcmg_core STATIC
    ${REPO_ROOT}/tcmg-globals.c
    ${REPO_ROOT}/tcmg-main.c
    ${REPO_ROOT}/tcmg-client.c
    ${REPO_ROOT}/tcmg-log.c
    ${REPO_ROOT}/tcmg-conf.c
    ${REPO_ROOT}/tcmg-failban.c
    ${REPO_ROOT}/tcmg-emu.c
    ${REPO_ROOT}/tcmg-srvid.c
    ${REPO_ROOT}/tcmg-net.c
    ${REPO_ROOT}/tcmg-platform.c
    ${REPO_ROOT}/cscrypt/crypto.c
    ${REPO_ROOT}/module-cccam.c
    ${REPO_ROOT}/module-newcamd.c
    ${REPO_ROOT}/webif/webif.c
    ${REPO_ROOT}/webif/webif-api.c
    ${REPO_ROOT}/webif/webif-common.c
    ${REPO_ROOT}/webif/webif-layout.c
    ${REPO_ROOT}/webif/webif-page-login.c
    ${REPO_ROOT}/webif/webif-page-status.c
    ${REPO_ROOT}/webif/webif-page-system.c
    ${REPO_ROOT}/webif/webif-page-users.c
    ${REPO_ROOT}/webif/webif-tvcas.c
)

target_include_directories(tcmg_core PUBLIC
    ${REPO_ROOT}
    ${REPO_ROOT}/cscrypt
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
