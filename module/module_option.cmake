###设置源文件
set(MODULE_MODULE_LIST 
    ${BUILD_MODULE_PATH}/alarm
    ${BUILD_MODULE_PATH}/config
    ${BUILD_MODULE_PATH}/comm
    ${BUILD_MODULE_PATH}/factory
    ${BUILD_MODULE_PATH}/mbuf
    ${BUILD_MODULE_PATH}/media
    ${BUILD_MODULE_PATH}/net
    ${BUILD_MODULE_PATH}/ota
    ${BUILD_MODULE_PATH}/sdcard
    ${BUILD_MODULE_PATH}/service
    ${BUILD_MODULE_PATH}/service/anjpri
    ${BUILD_MODULE_PATH}/sysmng
    )

### 设置头文件和源码路径的自动收集
set(MODULE_SRC_LIST "")
set(MODULE_INC_LIST "")
set(ANJ_MODULE_STATIC_TARGETS
    anjalarm
    anjconfig
    anjcomm
    anjfactory
    anjmbuf
    anjmedia
    anjnet
    anjota
    anjsdcard
    anjservice
    anjpri
    anjsysmng
    )

set(MODULE_LINK_DIR_LIST ${BUILD_PREBUILD_PATH}/common_libs/${BUILD_COMPILER}/lib/)
set(MODULE_LINK_LIBS softsn.a resample.a z.a ${BUILD_PREBUILD_PATH}/openssl-1.0.2d/${BUILD_COMPILER}/lib/libcrypto.a ${BUILD_PREBUILD_PATH}/libjpeg/${BUILD_COMPILER}/lib/libjpeg.a)
list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/service/ftpemail/inc)
set(ANJ_LINK_KEEP_SYMBOLS
  anj_module_keep_anj_alarm
  anj_module_keep_anj_config
  anj_module_keep_anj_audio
  anj_module_keep_anj_video
  anj_module_keep_anj_osd
  anj_module_keep_anj_ispctl
  anj_module_keep_anj_net
  anj_module_keep_anj_sdcard
  anj_module_keep_anj_service
  anj_module_keep_anj_sysmng
)
list(APPEND MODULE_INC_LIST ${BUILD_PREBUILD_PATH}/libjpeg)

# snap 核心始终编译（无 soft/hard 时 API 返回失败）；backend 互斥可选
list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/snap)
list(APPEND ANJ_MODULE_STATIC_TARGETS anjsnap)
list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_module_keep_anj_snap)
list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/snap/inc)
if(MODULE_SNAP_SOFT)
  add_definitions("-DMODULE_SNAP_SOFT")
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/snap/snapsoft)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjsnapsoft)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_snap_soft_provider)
  list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/snap/snapsoft/inc)
endif()
if(MODULE_SNAP_HARD)
  add_definitions("-DMODULE_SNAP_HARD")
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/snap/snaphard)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjsnaphard)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_snap_hard_provider)
  list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/snap/snaphard/inc)
endif()

if(MODULE_PTZ)
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/ptz)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjptz)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_module_keep_anj_ptz)
  if(MODULE_PTZ_DRV)
    add_definitions("-D_PTZ_DRV_SUPPORT_")
    list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/ptz/ptzdrv)
    list(APPEND ANJ_MODULE_STATIC_TARGETS anjptzdrv)
    list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_ptzdrv_provider)
  endif()
  if(MODULE_PTZ_PWM)
    add_definitions("-D_PTZ_PWM_SUPPORT_")
	list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/ptz/ptzpwm)
    list(APPEND ANJ_MODULE_STATIC_TARGETS anjptzpwm)
    list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_ptzpwm_provider)
  endif()
  if(MODULE_PTZ_IIC)
    add_definitions("-D_PTZ_IIC_SUPPORT_")
    list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/ptz/ptziic)
    list(APPEND ANJ_MODULE_STATIC_TARGETS anjptziic)
    list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_ptziic_provider)
  endif()
endif()

list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/record)
list(APPEND ANJ_MODULE_STATIC_TARGETS anjrecord)

if(MODULE_SDCARD)
  add_definitions("-D_USE_MODULE_SDCARD_")
endif()

if(FLASH_TYPE_NAND)
  add_definitions("-D_USE_NAND_FLASH_")
endif()

if(MODULE_NFS)
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/nfs)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjnfs)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_module_keep_anj_nfs)
endif()

if(MODULE_MCU)
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/mcu)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjmcu)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_module_keep_anj_mcu)
endif()

if(MODULE_AOV)
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/aov)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjaov)
  add_definitions("-D_USE_MODULE_AOV_")
endif()

if(MODULE_BATTERY)
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/battery)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjbattery)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_module_keep_anj_battery)
  add_definitions("-D_USE_MODULE_BATTERY_")
endif()

if(MODULE_GYRO)
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/gyro)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjgyro)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_module_keep_anj_gyro)
  add_definitions("-D_USE_MODULE_GYRO_")
  if(MODULE_GYRO_ICM42607P)
    list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/gyro/icm42607p)
    list(APPEND ANJ_MODULE_STATIC_TARGETS anjgyroicm42607p)
    list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_gyro_icm42607p_provider)
  endif()
endif()

if(MODULE_BLUETOOTH)
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/ble)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjble)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_ble_provider)
  add_definitions("-D_USE_MODULE_BLUETOOTH_")
  set(ANJ_BLE_BUNDLE_LIBS
    ${BUILD_PREBUILD_PATH}/ble/${BUILD_COMPILER}/lib/liblbh.a
    ${BUILD_PREBUILD_PATH}/ble/${BUILD_COMPILER}/lib/liblbh_xm1223.a
  )
  list(APPEND MODULE_INC_LIST ${BUILD_PREBUILD_PATH}/ble/inc/)
  list(APPEND MODULE_LINK_DIR_LIST ${BUILD_PREBUILD_PATH}/ble/${BUILD_COMPILER}/lib/)
  list(APPEND MODULE_LINK_LIBS ${ANJ_BLE_BUNDLE_LIBS})
endif()

if(MODULE_SMART)
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/smart)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjsmart)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_module_keep_anj_smart)
  if(MODULE_SMART_MD)
    add_definitions("-D_USE_SMART_MD_")
    list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/smart/md)
    list(APPEND ANJ_MODULE_STATIC_TARGETS anjsmartmd)
    list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_smart_md_provider)
  endif()
  if(MODULE_SMART_PD)
    add_definitions("-D_USE_SMART_PD_")
    list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/smart/pd)
    list(APPEND ANJ_MODULE_STATIC_TARGETS anjsmartpd)
    list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_smart_pd_provider anj_keep_mw_smart_pd_provider)
  endif()
  if(MODULE_SMART_FD)
    list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/smart/fd)
    list(APPEND ANJ_MODULE_STATIC_TARGETS anjsmartfd)
    list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_smart_fd_provider anj_keep_mw_smart_fd_provider)
  endif()
  if(MODULE_SMART_PVD)
    list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/smart/pvd)
    list(APPEND ANJ_MODULE_STATIC_TARGETS anjsmartpvd)
    list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_smart_pvd_provider anj_keep_mw_smart_pvd_provider)
  endif()
endif()

if(MODULE_MEDIA_AUDIO)
    list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/media/${BUILD_SRC}/audio_utils)
    list(APPEND ANJ_MODULE_STATIC_TARGETS anjmediaaudio)
    list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/media/${BUILD_SRC}/audio_utils)
    list(APPEND MODULE_SRC_LIST ${BUILD_MODULE_PATH}/media/${BUILD_SRC}/audio_utils)
	if(MODULE_MEDIA_AAC)
		list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/media/${BUILD_SRC}/audio_utils/aac)
		list(APPEND ANJ_MODULE_STATIC_TARGETS anjmediaaac)
		list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_audio_aac_codec_provider)
		set(ANJ_MEDIA_AAC_BUNDLE_LIBS
		  ${BUILD_PREBUILD_PATH}/common_libs/${BUILD_COMPILER}/lib/libfdk-aac.a
		)
		list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/media/${BUILD_SRC}/audio_utils/aac/fdk-aac)
		list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/media/${BUILD_SRC}/audio_utils/aac)
		list(APPEND MODULE_SRC_LIST ${BUILD_MODULE_PATH}/media/${BUILD_SRC}/audio_utils/aac)
		add_compile_definitions(FDKFILE=void)
		list(APPEND MODULE_LINK_LIBS ${ANJ_MEDIA_AAC_BUNDLE_LIBS})
	endif()
	if(MODULE_MEDIA_MP3)
		list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/media/${BUILD_SRC}/audio_utils/mp3)
		list(APPEND ANJ_MODULE_STATIC_TARGETS anjmediamp3)
		list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/media/${BUILD_SRC}/audio_utils/mp3/lame)
		list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/media/${BUILD_SRC}/audio_utils/mp3)
		list(APPEND MODULE_SRC_LIST ${BUILD_MODULE_PATH}/media/${BUILD_SRC}/audio_utils/mp3)
		set(ANJ_MEDIA_MP3_BUNDLE_LIBS
		  ${BUILD_PREBUILD_PATH}/common_libs/${BUILD_COMPILER}/lib/libmp3lame.a
		)
		list(APPEND MODULE_LINK_LIBS ${ANJ_MEDIA_MP3_BUNDLE_LIBS})
	endif()

	if(MODULE_AUDIO_ALGO_AEC)
		list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_mw_media_audio_aec_provider)
	endif()

endif()

if(MODULE_NET_WIRE)
  add_definitions("-D_USE_MODULE_WIRE_")
endif()
if(MODULE_NET_WIFI)
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/net/wifi)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjnetwifi)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_net_wifi_provider)
endif()
if(MODULE_NET_4G)
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/net/4g)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjnet4g)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_net_4g_provider)
  add_definitions("-D_USE_MODULE_4G_")
endif()

if(MODULE_SREVICE_28181)
  add_definitions("-D_USE_MODULE_GB28181_")
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/service/gb28181)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjgb28181)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_gb28181_provider)
  set(ANJ_GB28181_BUNDLE_LIBS
    ${BUILD_PREBUILD_PATH}/gb28181/${BUILD_COMPILER}/lib/libgb28181.a
  )

  list(APPEND MODULE_INC_LIST ${BUILD_PREBUILD_PATH}/gb28181/inc/)
  list(APPEND MODULE_LINK_DIR_LIST ${BUILD_PREBUILD_PATH}/gb28181/${BUILD_COMPILER}/)
  list(APPEND MODULE_LINK_LIBS ${ANJ_GB28181_BUNDLE_LIBS})
endif()

if(MODULE_SERVICE_FTPEMAIL)
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/service/ftpemail)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjftpemail)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_ftpemail_provider)
endif()

if(MODULE_SERVICE_HTTP)
    add_definitions("-D_USE_MODULE_HTTP_")  
    list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/service/http)
    list(APPEND ANJ_MODULE_STATIC_TARGETS anjhttp)

    list(APPEND MODULE_INC_LIST ${BUILD_PREBUILD_PATH}/openssl-1.0.2d/)
    list(APPEND MODULE_LINK_DIR_LIST ${BUILD_PREBUILD_PATH}/openssl-1.0.2d/${BUILD_COMPILER}/lib/)

    list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/service/http/${BUILD_SRC}/cgi/inc/)
    list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/service/http/${BUILD_SRC}/hapi/inc/)
    list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/service/http/${BUILD_SRC}/http/inc/)
    list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/service/http/${BUILD_SRC}/unv/inc/)
    list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/service/http/${BUILD_SRC}/web_post/inc/)

    list(APPEND MODULE_SRC_LIST ${BUILD_MODULE_PATH}/service/http/${BUILD_SRC}/cgi/src/)
    list(APPEND MODULE_SRC_LIST ${BUILD_MODULE_PATH}/service/http/${BUILD_SRC}/hapi/src/)
    list(APPEND MODULE_SRC_LIST ${BUILD_MODULE_PATH}/service/http/${BUILD_SRC}/http/src/)
    list(APPEND MODULE_SRC_LIST ${BUILD_MODULE_PATH}/service/http/${BUILD_SRC}/unv/src/)
    list(APPEND MODULE_SRC_LIST ${BUILD_MODULE_PATH}/service/http/${BUILD_SRC}/web_post/src/)
endif()

if(MODULE_SREVICE_ANJWEB)
    add_definitions("-D_USE_MODULE_WEB_")  
    list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/service/anjweb)
    list(APPEND ANJ_MODULE_STATIC_TARGETS anjweb)
    list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_web_provider)
    set(ANJ_WEB_BUNDLE_LIBS
      ${BUILD_PREBUILD_PATH}/civetweb/${BUILD_COMPILER}/lib/libcivetweb.a
    )

    list(APPEND MODULE_INC_LIST ${BUILD_PREBUILD_PATH}/civetweb/inc/)
    list(APPEND MODULE_LINK_DIR_LIST ${BUILD_PREBUILD_PATH}/civetweb/${BUILD_COMPILER}/lib/)
    list(APPEND MODULE_LINK_LIBS ${ANJ_WEB_BUNDLE_LIBS})
endif()

if(MODULE_SREVICE_H5LIVE)
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/service/h5live)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjh5live)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_h5live_provider)
  add_definitions("-D_USE_MODULE_H5LIVE_")
  list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/service/h5live/inc)
endif()

if(MODULE_SREVICE_RTMP)
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/service/rtmp)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjrtmp)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_rtmp_provider)
  list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/service/rtmp/inc)
  list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/service/rtmp/inc/librtmp)
endif()

if(MODULE_SREVICE_HIK)
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/service/hik)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjhik)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_hik_provider)
  list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/service/hik/inc)
  add_definitions("-D_USE_MODULE_HIK_")
endif()

if(MODULE_SREVICE_ONVIF)
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/service/onvif)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjonvif)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_onvif_provider)
  list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/service/onvif/inc)
  add_definitions("-D_USE_MODULE_ONVIF_")

    if (ONVIF_VERSION_SIMPLE)
        add_definitions("-D_USE_ONVIF_SIMPLE_")
        add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=1)
        list(APPEND MODULE_SRC_LIST ${BUILD_MODULE_PATH}/service/onvif/${BUILD_SRC}/HappyTime_Simple/bm)
        list(APPEND MODULE_SRC_LIST ${BUILD_MODULE_PATH}/service/onvif/${BUILD_SRC}/HappyTime_Simple/onvif)
    endif()

    if (ONVIF_VERSION_FULL)
        add_definitions("-D_USE_ONVIF_FULL_")
        add_definitions("-DEPOLL")
        add_definitions("-DHTTPD")
        add_definitions("-DDEVICEIO_SUPPORT")
        add_definitions("-DMEDIA_SUPPORT")
        add_definitions("-DMEDIA2_SUPPORT")
        add_definitions("-DAUDIO_SUPPORT")
        add_definitions("-DIMAGE_SUPPORT")
        add_definitions("-DVIDEO_ANALYTICS")
        add_definitions("-DPTZ_SUPPORT")
        add_definitions("-DMPEG4_SUPPORT")
        add_definitions("-DDEVICEIO_SUPPORT")
        add_definitions("-DCREDENTIAL_SUPPORT")
        add_definitions("-DACCESS_RULES")
        add_definitions("-DSCHEDULE_SUPPORT")
        add_definitions("-DTHERMAL_SUPPORT")
        add_definitions("-DRECEIVER_SUPPORT")
        add_definitions("-DIPFILTER_SUPPORT")
        add_definitions("-DSTORAGE_SUPPORT")
        add_definitions("-DPROVISIONING_SUPPORT")
        add_definitions("-DGEOLOCATION_SUPPORT")
        add_definitions("-DDOT11_SUPPORT")

        list(APPEND MODULE_SRC_LIST ${BUILD_MODULE_PATH}/service/onvif/${BUILD_SRC}/HappyTime_Full/bm)
        list(APPEND MODULE_SRC_LIST ${BUILD_MODULE_PATH}/service/onvif/${BUILD_SRC}/HappyTime_Full/cgi)
        list(APPEND MODULE_SRC_LIST ${BUILD_MODULE_PATH}/service/onvif/${BUILD_SRC}/HappyTime_Full/http)
        list(APPEND MODULE_SRC_LIST ${BUILD_MODULE_PATH}/service/onvif/${BUILD_SRC}/HappyTime_Full/onvif)
        list(APPEND MODULE_SRC_LIST ${BUILD_MODULE_PATH}/service/onvif/${BUILD_SRC}/HappyTime_Full/parameters)
    endif()
endif()

if(MODULE_SREVICE_RTSP)
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/service/rtsp)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjrtsp)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_rtsp_provider)
  add_definitions("-D_USE_MODULE_RTSP_")
  
  # 添加RTSP模块的子目录
  if(IS_DIRECTORY ${BUILD_MODULE_PATH}/service/rtsp/${BUILD_SRC}/rtp_transport)
    list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/service/rtsp/${BUILD_SRC}/rtp_transport/include)
    list(APPEND MODULE_SRC_LIST ${BUILD_MODULE_PATH}/service/rtsp/${BUILD_SRC}/rtp_transport/)
  endif()
  
  if(IS_DIRECTORY ${BUILD_MODULE_PATH}/service/rtsp/${BUILD_SRC}/rtsp_server)
    list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/service/rtsp/${BUILD_SRC}/rtsp_server/anjrtsp/include)
    list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/service/rtsp/${BUILD_SRC}/rtsp_server/BasicUsageEnvironment/include)
    list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/service/rtsp/${BUILD_SRC}/rtsp_server/groupsock/include)
    list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/service/rtsp/${BUILD_SRC}/rtsp_server/liveMedia/include)
    list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/service/rtsp/${BUILD_SRC}/rtsp_server/UsageEnvironment/include)
    
    list(APPEND MODULE_SRC_LIST ${BUILD_MODULE_PATH}/service/rtsp/${BUILD_SRC}/rtsp_server/anjrtsp)
    list(APPEND MODULE_SRC_LIST ${BUILD_MODULE_PATH}/service/rtsp/${BUILD_SRC}/rtsp_server/BasicUsageEnvironment)
    list(APPEND MODULE_SRC_LIST ${BUILD_MODULE_PATH}/service/rtsp/${BUILD_SRC}/rtsp_server/groupsock)
    list(APPEND MODULE_SRC_LIST ${BUILD_MODULE_PATH}/service/rtsp/${BUILD_SRC}/rtsp_server/liveMedia)
    list(APPEND MODULE_SRC_LIST ${BUILD_MODULE_PATH}/service/rtsp/${BUILD_SRC}/rtsp_server/UsageEnvironment)
  endif()
endif()

if(MODULE_SREVICE_RTSP_LITE)
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/service/rtsp_lite)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjrtsplite)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_rtsp_lite_provider)
  add_definitions("-D_USE_MODULE_RTSP_")
  list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/service/rtsp_lite/${BUILD_SRC}/rtsp_server/rtp)
  list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/service/rtsp_lite/${BUILD_SRC}/rtsp_server/rtsp)
endif()

if(MODULE_SREVICE_ANJSER)
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/service/anjser)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjser)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_module_keep_anj_ser anj_module_keep_anj_bind)
  add_definitions("-D_USE_MODULE_ANJSER_")
  set(ANJ_SER_BUNDLE_LIBS
    ${BUILD_PREBUILD_PATH}/libser/${BUILD_COMPILER}/lib/libajupgrade.a
    ${BUILD_PREBUILD_PATH}/libser/${BUILD_COMPILER}/lib/libajp2papi.a
    ${BUILD_PREBUILD_PATH}/curl/${BUILD_COMPILER}/lib/libcurl.a
    ${BUILD_PREBUILD_PATH}/curl/${BUILD_COMPILER}/lib/libmbedtls.a
    ${BUILD_PREBUILD_PATH}/curl/${BUILD_COMPILER}/lib/libmbedx509.a
    ${BUILD_PREBUILD_PATH}/curl/${BUILD_COMPILER}/lib/libmbedcrypto.a
  )
  list(APPEND MODULE_INC_LIST ${BUILD_PREBUILD_PATH}/libser/inc/ ${BUILD_PREBUILD_PATH}/curl/inc/)
  list(APPEND MODULE_LINK_DIR_LIST ${BUILD_PREBUILD_PATH}/libser/${BUILD_COMPILER}/lib/ ${BUILD_PREBUILD_PATH}/curl/${BUILD_COMPILER}/lib/)
  list(APPEND MODULE_LINK_LIBS ${ANJ_SER_BUNDLE_LIBS})

  if(AIOT_SERVER)
    add_definitions("-D_USE_AIOT_SERVER_")
    list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/service/anjser/aiot)
    list(APPEND ANJ_MODULE_STATIC_TARGETS anjaiot)
    list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_aiot_provider)
    set(ANJ_AIOT_BUNDLE_LIBS
      ${BUILD_PREBUILD_PATH}/libaiot/${BUILD_COMPILER}/lib/libgt_terminal.a
      ${BUILD_PREBUILD_PATH}/libaiot/${BUILD_COMPILER}/lib/libgt_server.a
      ${BUILD_PREBUILD_PATH}/libaiot/${BUILD_COMPILER}/lib/libgt_doorbell.a
      ${BUILD_PREBUILD_PATH}/libaiot/${BUILD_COMPILER}/lib/libgt_network.a
      ${BUILD_PREBUILD_PATH}/libaiot/${BUILD_COMPILER}/lib/libgt_common.a
      ${BUILD_PREBUILD_PATH}/libaiot/${BUILD_COMPILER}/lib/libgt_slog.a
      ${BUILD_PREBUILD_PATH}/libaiot/${BUILD_COMPILER}/lib/libscudt.a
      ${BUILD_PREBUILD_PATH}/libaiot/${BUILD_COMPILER}/lib/librandDataEnc.a
    )
    list(APPEND MODULE_INC_LIST ${BUILD_PREBUILD_PATH}/libaiot/inc/)
    list(APPEND MODULE_LINK_DIR_LIST ${BUILD_PREBUILD_PATH}/libaiot/${BUILD_COMPILER}/lib/)
    list(APPEND MODULE_LINK_LIBS ${ANJ_AIOT_BUNDLE_LIBS})
    if(AIOT_CLOUD_STORAGE)
      list(APPEND MODULE_LINK_LIBS ${BUILD_PREBUILD_PATH}/libaiot/${BUILD_COMPILER}/lib/cloud/libgt_cloud.a)
    endif()
  endif()

  if(MODULE_CLOUD_STORAGE)
    add_definitions("-D_USE_CLOUD_")
    list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/service/anjser/cloud)
    list(APPEND ANJ_MODULE_STATIC_TARGETS anjcloud)
    list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_cloud_provider)

    if(CT_CLOUD_STORAGE)
      list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/service/anjser/cloud/ct_cloud)
      list(APPEND ANJ_MODULE_STATIC_TARGETS anjcloudct)
      list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_keep_cloud_ct_provider)
      set(ANJ_CLOUD_CT_BUNDLE_LIBS
        ${BUILD_PREBUILD_PATH}/ctcloud/${BUILD_COMPILER}/lib/libct_deviceinf.a
        ${BUILD_PREBUILD_PATH}/ctcloud/${BUILD_COMPILER}/lib/libct_server.a
        ${BUILD_PREBUILD_PATH}/ctcloud/${BUILD_COMPILER}/lib/libct_cloud.a
        ${BUILD_PREBUILD_PATH}/ctcloud/${BUILD_COMPILER}/lib/libct_slog.a
        ${BUILD_PREBUILD_PATH}/ctcloud/${BUILD_COMPILER}/lib/libct_network.a
        ${BUILD_PREBUILD_PATH}/ctcloud/${BUILD_COMPILER}/lib/libct_common.a
      )
      list(APPEND MODULE_INC_LIST ${BUILD_PREBUILD_PATH}/ctcloud/inc/)
      list(APPEND MODULE_LINK_DIR_LIST ${BUILD_PREBUILD_PATH}/ctcloud/${BUILD_COMPILER}/lib/)
      list(APPEND MODULE_LINK_LIBS ${ANJ_CLOUD_CT_BUNDLE_LIBS})
      add_definitions("-D_USE_CTCLOUD_")
    endif()
  endif()
endif()

if(MODULE_ZXING)
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/zxing)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjzxing)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_module_keep_anj_zxing)
  set(ANJ_ZXING_BUNDLE_LIBS
    ${BUILD_PREBUILD_PATH}/zxing/${BUILD_COMPILER}/lib/libzxing.a
  )
  list(APPEND MODULE_INC_LIST ${BUILD_PREBUILD_PATH}/zxing/inc/)
  list(APPEND MODULE_LINK_DIR_LIST ${BUILD_PREBUILD_PATH}/zxing/${BUILD_COMPILER}/lib/)
  list(APPEND MODULE_LINK_LIBS ${ANJ_ZXING_BUNDLE_LIBS})
  set_source_files_properties(${BUILD_MODULE_PATH}/zxing/${BUILD_SRC}/anj_zxing.cpp PROPERTIES COMPILE_FLAGS "-std=gnu++17")
endif()

if(MODULE_WAVE)
  list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/wave)
  list(APPEND ANJ_MODULE_STATIC_TARGETS anjwave)
  list(APPEND ANJ_LINK_KEEP_SYMBOLS anj_module_keep_anj_wave)
  set(ANJ_WAVE_BUNDLE_LIBS
    ${BUILD_PREBUILD_PATH}/wave/${BUILD_COMPILER}/lib/libsoundwave.a
  )
  list(APPEND MODULE_INC_LIST ${BUILD_PREBUILD_PATH}/wave/inc/)
  list(APPEND MODULE_LINK_DIR_LIST ${BUILD_PREBUILD_PATH}/wave/${BUILD_COMPILER}/lib/)
  list(APPEND MODULE_LINK_LIBS ${ANJ_WAVE_BUNDLE_LIBS})
endif()


# 遍历所有模块路径
foreach(module ${MODULE_MODULE_LIST})
  # 收集每个模块的src目录（如果存在）
  if(IS_DIRECTORY ${module}/src)
    list(APPEND MODULE_SRC_LIST ${module}/${BUILD_SRC}/)
  endif()

  # 收集每个模块的inc目录（如果存在）
  if(IS_DIRECTORY ${module}/inc)
    list(APPEND MODULE_INC_LIST ${module}/${BUILD_INC}/)
  endif()
endforeach()

if(IS_DIRECTORY ${BUILD_MODULE_PATH}/inc)
  list(APPEND MODULE_INC_LIST ${BUILD_MODULE_PATH}/${BUILD_INC}/)
endif()

list(REMOVE_DUPLICATES MODULE_INC_LIST)
list(REMOVE_DUPLICATES BUILD_MDW_INC_LIST)
list(REMOVE_DUPLICATES BUILD_MDW_PLATFORMS_INC_PATH)
list(REMOVE_DUPLICATES MODULE_LINK_DIR_LIST)
list(REMOVE_DUPLICATES BUILD_MDW_PLATFORMS_LINK_PATH)
list(REMOVE_DUPLICATES ANJ_MODULE_STATIC_TARGETS)
list(REMOVE_DUPLICATES ANJ_LINK_KEEP_SYMBOLS)

include_directories(${BUILD_APP_PATH}/${BUILD_INC})
include_directories(${BUILD_PROJECT_TARGET_PATH}/${BUILD_INC})
include_directories(${BUILD_MODULE_PATH}/${BUILD_INC})

foreach (MODULE_INC_LIST_ ${MODULE_INC_LIST})
  include_directories(${MODULE_INC_LIST_})
endforeach()

foreach (BUILD_MDW_INC_LIST_ ${BUILD_MDW_INC_LIST})
  include_directories(${BUILD_MDW_INC_LIST_})
endforeach()

foreach (BUILD_MDW_PLATFORMS_INC_PATH_ ${BUILD_MDW_PLATFORMS_INC_PATH})
  include_directories(${BUILD_MDW_PLATFORMS_INC_PATH_})
endforeach()

foreach (MODULE_LINK_DIR_LIST_ ${MODULE_LINK_DIR_LIST})
  link_directories(${MODULE_LINK_DIR_LIST_})
endforeach()

foreach (BUILD_MDW_PLATFORMS_LINK_PATH_ ${BUILD_MDW_PLATFORMS_LINK_PATH})
  link_directories(${BUILD_MDW_PLATFORMS_LINK_PATH_})
endforeach()

function(anj_module_prepare_output target_name rel_output_dir)
  set(module_output_dir ${PROJECT_SOURCE_DIR}/out/lib/${rel_output_dir})
  set_target_properties(${target_name} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${module_output_dir}
  )
endfunction()

function(anj_module_copy_archives target_name rel_output_dir)
  set(module_output_dir ${PROJECT_SOURCE_DIR}/out/lib/${rel_output_dir})
  foreach(archive_path ${ARGN})
    add_custom_command(TARGET ${target_name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory ${module_output_dir}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${archive_path} ${module_output_dir}/
      VERBATIM
    )
  endforeach()
endfunction()

set(ANJ_COMMON_LIB_OUTPUT_DIR ${PROJECT_SOURCE_DIR}/out/lib/common)
add_custom_target(anj_module_common_libs ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory ${ANJ_COMMON_LIB_OUTPUT_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${BUILD_PREBUILD_PATH}/common_libs/${BUILD_COMPILER}/lib/libsoftsn.a ${ANJ_COMMON_LIB_OUTPUT_DIR}/
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${BUILD_PREBUILD_PATH}/common_libs/${BUILD_COMPILER}/lib/libresample.a ${ANJ_COMMON_LIB_OUTPUT_DIR}/
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${BUILD_PREBUILD_PATH}/common_libs/${BUILD_COMPILER}/lib/libz.a ${ANJ_COMMON_LIB_OUTPUT_DIR}/
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${BUILD_PREBUILD_PATH}/openssl-1.0.2d/${BUILD_COMPILER}/lib/libcrypto.a ${ANJ_COMMON_LIB_OUTPUT_DIR}/
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${BUILD_PREBUILD_PATH}/libjpeg/${BUILD_COMPILER}/lib/libjpeg.a ${ANJ_COMMON_LIB_OUTPUT_DIR}/
  VERBATIM
)

list(REMOVE_DUPLICATES MODULE_MODULE_LIST)
if(NOT DEFINED ANJ_BUILD_MODULE_SUBDIRS)
  set(ANJ_BUILD_MODULE_SUBDIRS ON)
endif()

if(ANJ_BUILD_ALL_MODULES)
  file(GLOB_RECURSE ANJ_ALL_MODULE_CMAKE_FILES RELATIVE ${BUILD_MODULE_PATH} ${BUILD_MODULE_PATH}/*/CMakeLists.txt)
  foreach(ANJ_MODULE_CMAKE_FILE ${ANJ_ALL_MODULE_CMAKE_FILES})
    get_filename_component(ANJ_MODULE_DIR ${ANJ_MODULE_CMAKE_FILE} DIRECTORY)
    list(APPEND MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/${ANJ_MODULE_DIR})
  endforeach()
  list(REMOVE_DUPLICATES MODULE_MODULE_LIST)
  if(NOT MODULE_AOV)
    list(REMOVE_ITEM MODULE_MODULE_LIST ${BUILD_MODULE_PATH}/aov)
  endif()
endif()

if(ANJ_BUILD_MODULE_SUBDIRS)
  foreach(module_dir ${MODULE_MODULE_LIST})
    if(EXISTS "${module_dir}/CMakeLists.txt")
      add_subdirectory(${module_dir})
    endif()
  endforeach()
endif()
