set(TARGET_NAME anjrtmp)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/out/lib/)

set(RTMP_SRC_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/src/anj_rtmp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/anj_rtmp_ctrl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/anj_rtmp_media.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/anj_rtmp_mux.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/anj_rtmp_session.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/librtmp/amf0.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/librtmp/rtmp-chunk-header.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/librtmp/rtmp-chunk-read.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/librtmp/rtmp-chunk-write.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/librtmp/rtmp-client.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/librtmp/rtmp-control-handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/librtmp/rtmp-control-message.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/librtmp/rtmp-event.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/librtmp/rtmp-handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/librtmp/rtmp-handshake.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/librtmp/rtmp-invoke-handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/librtmp/rtmp-netconnection.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/librtmp/rtmp-netstream.c
)

add_library(${TARGET_NAME} STATIC ${RTMP_SRC_LIST})
anj_module_prepare_output(${TARGET_NAME} "service/rtmp")

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/librtmp
)

foreach (MODULE_INC_LIST_ ${MODULE_INC_LIST})
    target_include_directories(${TARGET_NAME} PRIVATE ${MODULE_INC_LIST_})
endforeach()
