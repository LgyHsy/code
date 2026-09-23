set(TARGET_NAME anjnetwifi)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/out/lib/)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC_LIST)
add_library(${TARGET_NAME} STATIC ${SRC_LIST})
anj_module_prepare_output(${TARGET_NAME} "net/wifi")

foreach (MODULE_INC_LIST_ ${MODULE_INC_LIST})
    target_include_directories(${TARGET_NAME} PRIVATE ${MODULE_INC_LIST_})
endforeach()
