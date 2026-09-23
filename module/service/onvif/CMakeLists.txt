set(TARGET_NAME anjonvif)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/out/lib/)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC_LIST)

if(ONVIF_VERSION_SIMPLE)
    aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/HappyTime_Simple/bm SRC_LIST)
    aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/HappyTime_Simple/onvif SRC_LIST)
endif()

if(ONVIF_VERSION_FULL)
    aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/HappyTime_Full/bm SRC_LIST)
    aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/HappyTime_Full/cgi SRC_LIST)
    aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/HappyTime_Full/http SRC_LIST)
    aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/HappyTime_Full/onvif SRC_LIST)
    aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/HappyTime_Full/parameters SRC_LIST)
endif()

add_library(${TARGET_NAME} STATIC ${SRC_LIST})
anj_module_prepare_output(${TARGET_NAME} "service/onvif")

if(ONVIF_VERSION_FULL)
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/HappyTime_Full/bm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/HappyTime_Full/cgi
        ${CMAKE_CURRENT_SOURCE_DIR}/src/HappyTime_Full/http
        ${CMAKE_CURRENT_SOURCE_DIR}/src/HappyTime_Full/onvif
        ${CMAKE_CURRENT_SOURCE_DIR}/src/HappyTime_Full/openssl
        ${CMAKE_CURRENT_SOURCE_DIR}/src/HappyTime_Full/parameters)
elseif(ONVIF_VERSION_SIMPLE)
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/HappyTime_Simple/bm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/HappyTime_Simple/onvif)
endif()

foreach (MODULE_INC_LIST_ ${MODULE_INC_LIST})
    target_include_directories(${TARGET_NAME} PRIVATE ${MODULE_INC_LIST_})
endforeach()
