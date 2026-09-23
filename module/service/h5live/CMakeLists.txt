set(TARGET_NAME anjh5live)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/out/lib/)

set(H5LIVE_SRC_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/src/anj_h5live.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/anj_h5_ws_server.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/anj_h5_stream.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/anj_h5_media.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/anj_h5_auth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/anj_h5_playback.c
    ${CMAKE_CURRENT_SOURCE_DIR}/wslay/base64-encode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/wslay/sha1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/wslay/sha1-compress.c
    ${CMAKE_CURRENT_SOURCE_DIR}/wslay/write-be32.c
    ${CMAKE_CURRENT_SOURCE_DIR}/wslay/wslay_event.c
    ${CMAKE_CURRENT_SOURCE_DIR}/wslay/wslay_frame.c
    ${CMAKE_CURRENT_SOURCE_DIR}/wslay/wslay_net.c
    ${CMAKE_CURRENT_SOURCE_DIR}/wslay/wslay_queue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/wslay/wslay_stack.c
)

add_library(${TARGET_NAME} STATIC ${H5LIVE_SRC_LIST})
anj_module_prepare_output(${TARGET_NAME} "service/h5live")

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/wslay
)

foreach (MODULE_INC_LIST_ ${MODULE_INC_LIST})
    target_include_directories(${TARGET_NAME} PRIVATE ${MODULE_INC_LIST_})
endforeach()
