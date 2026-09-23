set(TARGET_NAME anjftpemail)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/out/lib/)

set(SRC_LIST ${CMAKE_CURRENT_SOURCE_DIR}/src/anj_ftpemail.c)
add_library(${TARGET_NAME} STATIC ${SRC_LIST})
anj_module_prepare_output(${TARGET_NAME} "service/ftpemail")

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${PROJECT_SOURCE_DIR}/module/service/inc
)

foreach (MODULE_INC_LIST_ ${MODULE_INC_LIST})
    target_include_directories(${TARGET_NAME} PRIVATE ${MODULE_INC_LIST_})
endforeach()
