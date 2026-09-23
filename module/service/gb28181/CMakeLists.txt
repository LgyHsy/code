set(TARGET_NAME anjgb28181)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/out/lib/)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC_LIST)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/proc/src SRC_LIST)
add_library(${TARGET_NAME} STATIC ${SRC_LIST})
anj_module_prepare_output(${TARGET_NAME} "service/gb28181")
anj_module_copy_archives(${TARGET_NAME} "service/gb28181"
    ${ANJ_GB28181_BUNDLE_LIBS}
)
target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/proc/inc
    ${PROJECT_SOURCE_DIR}/module/service/inc
    ${PROJECT_SOURCE_DIR}/module/alarm/inc
)

foreach (MODULE_INC_LIST_ ${MODULE_INC_LIST})
    target_include_directories(${TARGET_NAME} PRIVATE ${MODULE_INC_LIST_})
endforeach()
