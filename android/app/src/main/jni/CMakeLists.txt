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
    ${REPO_ROOT}/src/core/client_state.c
    ${REPO_ROOT}/src/core/config_state.c
    ${REPO_ROOT}/src/core/runtime_state.c
    ${REPO_ROOT}/src/main.c
    ${REPO_ROOT}/src/client/client.c
    ${REPO_ROOT}/src/log/log.c
    ${REPO_ROOT}/src/config/config.c
    ${REPO_ROOT}/src/config/config_global.c
    ${REPO_ROOT}/src/config/config_parse.c
    ${REPO_ROOT}/src/config/config_reader.c
    ${REPO_ROOT}/src/config/config_user.c
    ${REPO_ROOT}/src/config/config_validate.c
    ${REPO_ROOT}/src/config/config_write.c
    ${REPO_ROOT}/src/config/runtime_access.c
    ${REPO_ROOT}/src/account/account.c
    ${REPO_ROOT}/src/account/auth.c
    ${REPO_ROOT}/src/account/policy.c
    ${REPO_ROOT}/src/account/session.c
    ${REPO_ROOT}/src/stats/account_stats.c
    ${REPO_ROOT}/src/security/failban.c
    ${REPO_ROOT}/src/security/antishare.c
    ${REPO_ROOT}/src/emu/emu.c
    ${REPO_ROOT}/src/srvid/srvid.c
    ${REPO_ROOT}/src/net/net.c
    ${REPO_ROOT}/src/platform/platform.c
    ${REPO_ROOT}/src/pcsc/pcsc.c
    ${REPO_ROOT}/src/reader/reader.c
    ${REPO_ROOT}/src/reader/rules.c
    ${REPO_ROOT}/src/reader/registry.c
    ${REPO_ROOT}/src/reader/adapters.c
    ${REPO_ROOT}/src/reader/result.c
    ${REPO_ROOT}/src/reader/protocols/cccam.c
    ${REPO_ROOT}/src/reader/protocols/newcamd.c
    ${REPO_ROOT}/src/reader/protocols/cs378x.c
    ${REPO_ROOT}/src/cache/cache_state.c
    ${REPO_ROOT}/src/cache/cw_cache.c
    ${REPO_ROOT}/src/crypto/crypto.c
    ${REPO_ROOT}/src/crypto/sha1.c
    ${REPO_ROOT}/src/crypto/newcamd_des.c
    ${REPO_ROOT}/src/crypto/aes128.c
    ${REPO_ROOT}/src/proto/cccam.c
    ${REPO_ROOT}/src/proto/newcamd.c
    ${REPO_ROOT}/src/proto/camd35.c
    ${REPO_ROOT}/src/proto/camd35_server.c
    ${REPO_ROOT}/src/ecm/request.c
    ${REPO_ROOT}/src/ecm/policy.c
    ${REPO_ROOT}/src/ecm/pipeline.c
    ${REPO_ROOT}/src/session/session.c
    ${REPO_ROOT}/src/proto/registry.c
    ${REPO_ROOT}/src/proto/server.c
    ${REPO_ROOT}/webif/server.c
    ${REPO_ROOT}/webif/core.c
    ${REPO_ROOT}/webif/pages/login.c
    ${REPO_ROOT}/webif/pages/status.c
    ${REPO_ROOT}/webif/pages/readers.c
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
    ${REPO_ROOT}/webif/api/pcsc.c
    ${REPO_ROOT}/webif/api/readers.c
    ${REPO_ROOT}/webif/service/account_service.c
    ${REPO_ROOT}/webif/service/client_service.c
    ${REPO_ROOT}/webif/service/config_service.c
    ${REPO_ROOT}/webif/service/reader_service.c
    ${REPO_ROOT}/webif/service/security_service.c
)

target_include_directories(tcmg_core PUBLIC
    ${REPO_ROOT}
    ${REPO_ROOT}/src
    ${REPO_ROOT}/webif
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(tcmg_jni SHARED
    tcmg_jni_bridge.c
)

target_include_directories(tcmg_jni PRIVATE
    ${REPO_ROOT}
    ${REPO_ROOT}/src
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
