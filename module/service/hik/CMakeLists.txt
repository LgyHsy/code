set(TARGET_NAME anjhik)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/out/lib/)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC_LIST)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/sadp SRC_LIST)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/netcmd SRC_LIST)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/des SRC_LIST)

add_library(${TARGET_NAME} STATIC ${SRC_LIST})
anj_module_prepare_output(${TARGET_NAME} "service/hik")

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sadp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/netcmd
    ${CMAKE_CURRENT_SOURCE_DIR}/src/des)

foreach (MODULE_INC_LIST_ ${MODULE_INC_LIST})
    target_include_directories(${TARGET_NAME} PRIVATE ${MODULE_INC_LIST_})
endforeach()
