set(TARGET_NAME anjmediaaac)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/out/lib/)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)
add_library(${TARGET_NAME} STATIC ${SRC_LIST})
anj_module_prepare_output(${TARGET_NAME} "media/audio/aac")
anj_module_copy_archives(${TARGET_NAME} "media/audio/aac"
    ${ANJ_MEDIA_AAC_BUNDLE_LIBS}
)
target_compile_definitions(${TARGET_NAME} PRIVATE FDKFILE=void)

foreach (MODULE_INC_LIST_ ${MODULE_INC_LIST})
    target_include_directories(${TARGET_NAME} PRIVATE ${MODULE_INC_LIST_})
endforeach()
