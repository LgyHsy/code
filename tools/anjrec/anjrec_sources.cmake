# Read-only module/middleware sources for anjrec (aligned with build_recovery).
set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../)

set(ANJREC_COMM_SRC
    ${REPO_ROOT}/module/comm/src/element.c
    ${REPO_ROOT}/module/comm/src/cJSON_Utils.c
    ${REPO_ROOT}/module/comm/src/namedNodeMap.c
    ${REPO_ROOT}/module/comm/src/anj_comm.c
    ${REPO_ROOT}/module/comm/src/node.c
    ${REPO_ROOT}/module/comm/src/ixmlparser.c
    ${REPO_ROOT}/module/comm/src/anj_base64.c
    ${REPO_ROOT}/module/comm/src/cJSON.c
    ${REPO_ROOT}/module/comm/src/nodeList.c
    ${REPO_ROOT}/module/comm/src/eventhub.c
    ${REPO_ROOT}/module/comm/src/document.c
    ${REPO_ROOT}/module/comm/src/ixml.c
    ${REPO_ROOT}/module/comm/src/driver_interface.c
    ${REPO_ROOT}/module/comm/src/ixmlmembuf.c
    ${REPO_ROOT}/module/comm/src/attr.c
    ${REPO_ROOT}/module/comm/src/util_font.c
)

set(ANJREC_CONFIG_SRC
    ${REPO_ROOT}/module/config/src/anj_config.c
    ${REPO_ROOT}/module/config/src/anj_config_network.c
    ${REPO_ROOT}/module/config/src/anj_config_oem.c
    ${REPO_ROOT}/module/config/src/anj_config_platform.c
    ${REPO_ROOT}/module/config/src/anj_config_server.c
    ${REPO_ROOT}/module/config/src/anj_config_stream.c
    ${REPO_ROOT}/module/config/src/anj_config_system.c
    ${REPO_ROOT}/module/config/src/anj_config_version.c
)

set(ANJREC_NET_SRC
    ${REPO_ROOT}/module/net/src/anj_net.c
    ${REPO_ROOT}/module/net/wifi/src/anj_wifi.c
    ${REPO_ROOT}/module/net/wifi/src/iwlib.c
)

set(ANJREC_SERVICE_SRC
    ${REPO_ROOT}/module/service/src/file_sender.c
    ${REPO_ROOT}/module/service/src/user_auth.c
    ${REPO_ROOT}/module/service/src/anj_search.c
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/module/file_receiver.c
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/module/anj_service_cmd_xml.c
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/module/anj_service_upgrade.c
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/module/anj_service_sysctl_basic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/module/anj_service_sysctl_recovery.c
)

set(ANJREC_ANJPRI_SRC
    ${REPO_ROOT}/module/service/anjpri/src/anj_pri.c
    ${REPO_ROOT}/module/service/anjpri/src/anj_pri_cmd.c
)

set(ANJREC_ANJSER_SRC
    ${REPO_ROOT}/module/service/anjser/src/anj_bind.c
    ${REPO_ROOT}/module/service/anjser/src/anj_ser.c
    ${REPO_ROOT}/module/service/anjser/src/ota_update.c
)

set(ANJREC_AIOT_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/module/aiot/anj_aiot_recovery.c
)

set(ANJREC_SYSMNG_SRC
    ${REPO_ROOT}/module/sysmng/src/anj_sysmng.c
    ${REPO_ROOT}/module/sysmng/src/firmware_util.c
    ${REPO_ROOT}/module/sysmng/src/anj_sysctl.c
    ${REPO_ROOT}/module/sysmng/src/anj_systime.c
    ${REPO_ROOT}/module/sysmng/src/ntp_client.c
)

set(ANJREC_MW_COMM_SRC
    ${REPO_ROOT}/middleware/comm/src/anj_mw_net.c
    ${REPO_ROOT}/middleware/comm/src/sem_util.c
    ${REPO_ROOT}/middleware/comm/src/anj_mw_log.c
    ${REPO_ROOT}/middleware/comm/src/anj_mw_mem.c
    ${REPO_ROOT}/middleware/comm/src/media_util.c
    ${REPO_ROOT}/middleware/comm/src/anj_mw_str.c
    ${REPO_ROOT}/middleware/comm/src/anj_mw_file.c
    ${REPO_ROOT}/middleware/comm/src/aes.c
    ${REPO_ROOT}/middleware/comm/src/anj_mw_thread.c
    ${REPO_ROOT}/middleware/comm/src/anj_mw_crypt.c
    ${REPO_ROOT}/middleware/comm/src/anj_mw_ring_queue.c
    ${REPO_ROOT}/middleware/comm/src/anj_mw_mutex.c
    ${REPO_ROOT}/middleware/comm/src/anj_mw_time.c
    ${REPO_ROOT}/middleware/comm/src/anj_mw_comm.c
    ${REPO_ROOT}/middleware/comm/src/anj_mw_ping.c
    ${REPO_ROOT}/middleware/comm/src/anj_mw_icmp.c
    ${REPO_ROOT}/middleware/comm/src/protocol_queue.c
)

set(ANJREC_MW_HWCTRL_SRC
    ${REPO_ROOT}/middleware/hwctrl/src/anj_mw_hwctrl.c
)

# Platform hw sources (recovery keeps hw only).
if(BUILD_PLATFORMS_NAME STREQUAL "mstar_ifliegen")
    set(ANJREC_MW_PLATFORM_HW_SRC
        ${REPO_ROOT}/middleware/platforms/mstar_ifliegen/hw/src/anj_mw_watchdog.c
        ${REPO_ROOT}/middleware/platforms/mstar_ifliegen/hw/src/anj_mw_ircut.c
        ${REPO_ROOT}/middleware/platforms/mstar_ifliegen/hw/src/anj_mw_virtualdev.c
        ${REPO_ROOT}/middleware/platforms/mstar_ifliegen/hw/src/anj_mw_rtc.c
        ${REPO_ROOT}/middleware/platforms/mstar_ifliegen/hw/src/anj_mw_adc.c
        ${REPO_ROOT}/middleware/platforms/mstar_ifliegen/hw/src/anj_mw_pwm.c
        ${REPO_ROOT}/middleware/platforms/mstar_ifliegen/hw/src/anj_mw_gpio.c
        ${REPO_ROOT}/middleware/platforms/mstar_ifliegen/hw/src/anj_mw_gpiodev.c
        ${REPO_ROOT}/middleware/platforms/mstar_ifliegen/hw/src/anj_mw_i2c.c
    )
elseif(BUILD_PLATFORMS_NAME STREQUAL "mstar_i6c")
    set(ANJREC_MW_PLATFORM_HW_SRC
        ${REPO_ROOT}/middleware/platforms/mstar_i6c/hw/src/anj_mw_watchdog.c
        ${REPO_ROOT}/middleware/platforms/mstar_i6c/hw/src/anj_mw_ircut.c
        ${REPO_ROOT}/middleware/platforms/mstar_i6c/hw/src/anj_mw_virtualdev.c
        ${REPO_ROOT}/middleware/platforms/mstar_i6c/hw/src/anj_mw_rtc.c
        ${REPO_ROOT}/middleware/platforms/mstar_i6c/hw/src/anj_mw_adc.c
        ${REPO_ROOT}/middleware/platforms/mstar_i6c/hw/src/anj_mw_pwm.c
        ${REPO_ROOT}/middleware/platforms/mstar_i6c/hw/src/anj_mw_gpio.c
        ${REPO_ROOT}/middleware/platforms/mstar_i6c/hw/src/anj_mw_gpiodev.c
        ${REPO_ROOT}/middleware/platforms/mstar_i6c/hw/src/anj_mw_i2c.c
    )
elseif(BUILD_PLATFORMS_NAME STREQUAL "ts_5326")
    set(ANJREC_MW_PLATFORM_HW_SRC
        ${REPO_ROOT}/middleware/platforms/ts_5326/hw/src/anj_mw_watchdog.c
        ${REPO_ROOT}/middleware/platforms/ts_5326/hw/src/anj_mw_ircut.c
        ${REPO_ROOT}/middleware/platforms/ts_5326/hw/src/anj_mw_virtualdev.c
        ${REPO_ROOT}/middleware/platforms/ts_5326/hw/src/anj_mw_rtc.c
        ${REPO_ROOT}/middleware/platforms/ts_5326/hw/src/anj_mw_gpio.c
        ${REPO_ROOT}/middleware/platforms/ts_5326/hw/src/anj_mw_i2c.c
        ${REPO_ROOT}/middleware/platforms/ts_5326/hw/src/anj_mw_pwm.c
    )
else()
    message(FATAL_ERROR "Unsupported BUILD_PLATFORMS_NAME for anjrec: ${BUILD_PLATFORMS_NAME}")
endif()

set(ANJREC_LOCAL_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/anjrec_compat.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/anjrec_devinfo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/anjrec_version_match.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/anjrec_auto_ota.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/aiot_ota_register.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/aiot_cmd_recovery.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/anjrec_service.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/anjrec_module.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/anjrec_sysmng.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/flash/anjrec_flash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/flash/anjrec_uboot_env.c
)

set(ANJREC_ALL_SRC
    ${ANJREC_LOCAL_SRC}
    ${ANJREC_COMM_SRC}
    ${ANJREC_CONFIG_SRC}
    ${ANJREC_NET_SRC}
    ${ANJREC_SERVICE_SRC}
    ${ANJREC_ANJPRI_SRC}
    ${ANJREC_ANJSER_SRC}
    ${ANJREC_AIOT_SRC}
    ${ANJREC_SYSMNG_SRC}
    ${ANJREC_MW_COMM_SRC}
    ${ANJREC_MW_HWCTRL_SRC}
    ${ANJREC_MW_PLATFORM_HW_SRC}
)
