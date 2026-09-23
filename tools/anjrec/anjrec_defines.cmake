# Target-specific compile/link settings for anjrec.
# Recovery-specific behavior lives in vendor/ and anjrec_sysmng hooks (no BUILD_RECOVERY macro).

set(BUILD_SRC src)
set(BUILD_INC inc)

if(DEBUG STREQUAL "y")
    set(ANJREC_OPT_FLAGS -Wall -Wno-psabi -g -O0)
else()
    set(ANJREC_OPT_FLAGS -Wall -Wno-psabi -O3)
endif()

string(TOUPPER "${BUILD_PROJECT_NAME}" ANJREC_PROJECT_MACRO)
string(TOUPPER "${BUILD_PLATFORMS_NAME}" ANJREC_PLATFORM_MACRO)

set(ANJREC_COMPILE_DEFS
    ${ANJREC_PROJECT_MACRO}
    ${ANJREC_PLATFORM_MACRO}
    _USE_MODULE_ANJSER_
    _USE_AIOT_SERVER_
)

if(MODULE_NET_WIRE STREQUAL "y")
    list(APPEND ANJREC_COMPILE_DEFS _USE_MODULE_WIRE_)
endif()

if(MODULE_NET_WIFI STREQUAL "y")
    list(APPEND ANJREC_COMPILE_DEFS _USE_MODULE_WIFI_)
endif()

set(BUILD_PREBUILD_PATH ${REPO_ROOT}/prebuild)
set(BUILD_PROJECT_TARGET_PATH ${REPO_ROOT}/project/${BUILD_PROJECT_NAME})
set(BUILD_MDW_PLATFORMS_PATH ${REPO_ROOT}/middleware/platforms/${BUILD_PLATFORMS_NAME})
set(BUILD_MDW_PLATFORMS_HW_PATH ${BUILD_MDW_PLATFORMS_PATH}/hw)

set(ANJREC_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/module
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${REPO_ROOT}/app/inc
    ${BUILD_PROJECT_TARGET_PATH}/${BUILD_INC}
    ${REPO_ROOT}/module/inc
    ${REPO_ROOT}/module/media/inc
    ${REPO_ROOT}/middleware/media/inc
    ${REPO_ROOT}/middleware/rec/inc
    ${REPO_ROOT}/module/record/inc
    ${REPO_ROOT}/module/sdcard/inc
    ${REPO_ROOT}/module/factory/inc
    ${REPO_ROOT}/module/smart/inc
    ${REPO_ROOT}/module/mbuf/inc
    ${REPO_ROOT}/module/alarm/inc
    ${BUILD_PREBUILD_PATH}/libser/inc
    ${BUILD_PREBUILD_PATH}/curl/inc
    ${BUILD_PREBUILD_PATH}/libaiot/inc
    ${REPO_ROOT}/module/comm/inc
    ${REPO_ROOT}/module/config/inc
    ${REPO_ROOT}/module/net/inc
    ${REPO_ROOT}/module/service/inc
    ${REPO_ROOT}/module/service/anjpri/inc
    ${REPO_ROOT}/module/service/anjser/inc
    ${REPO_ROOT}/module/service/anjser/aiot/inc
    ${REPO_ROOT}/module/sysmng/inc
    ${REPO_ROOT}/module/net/wifi/inc
    ${REPO_ROOT}/middleware/comm/inc
    ${REPO_ROOT}/middleware/hwctrl/inc
    ${BUILD_MDW_PLATFORMS_PATH}/${BUILD_INC}
    ${BUILD_MDW_PLATFORMS_HW_PATH}/${BUILD_INC}
)

set(ANJREC_LINK_DIRS
    ${BUILD_PREBUILD_PATH}/common_libs/${BUILD_COMPILER}/lib
    ${BUILD_PREBUILD_PATH}/libser/${BUILD_COMPILER}/lib
    ${BUILD_PREBUILD_PATH}/curl/${BUILD_COMPILER}/lib
    ${BUILD_PREBUILD_PATH}/libaiot/${BUILD_COMPILER}/lib
)

set(ANJREC_LINK_LIBS
    softsn
    z
    ${BUILD_PREBUILD_PATH}/openssl-1.0.2d/${BUILD_COMPILER}/lib/libcrypto.a
    ${BUILD_PREBUILD_PATH}/libser/${BUILD_COMPILER}/lib/libajupgrade.a
    ${BUILD_PREBUILD_PATH}/libser/${BUILD_COMPILER}/lib/libajp2papi.a
    ${BUILD_PREBUILD_PATH}/curl/${BUILD_COMPILER}/lib/libcurl.a
    ${BUILD_PREBUILD_PATH}/curl/${BUILD_COMPILER}/lib/libmbedtls.a
    ${BUILD_PREBUILD_PATH}/curl/${BUILD_COMPILER}/lib/libmbedx509.a
    ${BUILD_PREBUILD_PATH}/curl/${BUILD_COMPILER}/lib/libmbedcrypto.a
    ${BUILD_PREBUILD_PATH}/libaiot/${BUILD_COMPILER}/lib/libgt_terminal.a
    ${BUILD_PREBUILD_PATH}/libaiot/${BUILD_COMPILER}/lib/libgt_server.a
    ${BUILD_PREBUILD_PATH}/libaiot/${BUILD_COMPILER}/lib/cloud/libgt_cloud.a
    ${BUILD_PREBUILD_PATH}/libaiot/${BUILD_COMPILER}/lib/libgt_doorbell.a
    ${BUILD_PREBUILD_PATH}/libaiot/${BUILD_COMPILER}/lib/libgt_network.a
    ${BUILD_PREBUILD_PATH}/libaiot/${BUILD_COMPILER}/lib/libgt_common.a
    ${BUILD_PREBUILD_PATH}/libaiot/${BUILD_COMPILER}/lib/libgt_slog.a
    ${BUILD_PREBUILD_PATH}/libaiot/${BUILD_COMPILER}/lib/libscudt.a
    ${BUILD_PREBUILD_PATH}/libaiot/${BUILD_COMPILER}/lib/librandDataEnc.a
    stdc++
    m
    pthread
    dl
)

set(ANJREC_LINK_KEEP_SYMBOLS
    anj_module_keep_anj_config
    anj_module_keep_anj_net
    anj_module_keep_anj_service
    anj_module_keep_anj_ser
    anj_module_keep_anj_bind
    anj_module_keep_anj_sysmng
    anj_keep_net_wifi_provider
    anj_keep_aiot_provider
)

set(ANJREC_LINK_KEEP_FLAGS "")
foreach(ANJREC_KEEP_SYM ${ANJREC_LINK_KEEP_SYMBOLS})
    list(APPEND ANJREC_LINK_KEEP_FLAGS "-Wl,--undefined=${ANJREC_KEEP_SYM}")
endforeach()
