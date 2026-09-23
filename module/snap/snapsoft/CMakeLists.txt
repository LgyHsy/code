set(TARGET_NAME anjsnapsoft)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/out/lib/)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC_LIST)
add_library(${TARGET_NAME} STATIC ${SRC_LIST})
anj_module_prepare_output(${TARGET_NAME} "snap/snapsoft")

foreach (MODULE_INC_LIST_ ${MODULE_INC_LIST})
    target_include_directories(${TARGET_NAME} PRIVATE ${MODULE_INC_LIST_})
endforeach()

foreach (BUILD_MDW_INC_LIST_ ${BUILD_MDW_INC_LIST})
    target_include_directories(${TARGET_NAME} PRIVATE ${BUILD_MDW_INC_LIST_})
endforeach()

foreach (BUILD_MDW_PLATFORMS_INC_PATH_ ${BUILD_MDW_PLATFORMS_INC_PATH})
    target_include_directories(${TARGET_NAME} PRIVATE ${BUILD_MDW_PLATFORMS_INC_PATH_})
endforeach()
