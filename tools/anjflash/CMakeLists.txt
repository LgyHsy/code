cmake_minimum_required(VERSION 3.10)
PROJECT(anjflash)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/out/bin/)

set(ANJFLASH_PLATFORM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/platform/${BUILD_PLATFORMS_NAME})
set(ANJFLASH_PLATFORM_SRC ${ANJFLASH_PLATFORM_DIR}/src/anjflash_mmap.c)

if(NOT EXISTS ${ANJFLASH_PLATFORM_SRC})
    message(FATAL_ERROR "anjflash platform source not found: ${ANJFLASH_PLATFORM_SRC}")
endif()

set(SRC_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/src/anjflash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../anjrec/src/flash/anjrec_uboot_env.c
    ${ANJFLASH_PLATFORM_SRC}
)

add_executable(${PROJECT_NAME} ${SRC_LIST})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../anjrec/inc
    ${BUILD_MDW_PLATFORMS_INC_PATH}
    ${BUILD_PROJECT_TARGET_PATH}/${BUILD_INC}
)

foreach(ANJ_FLASH_LINK_DIR ${BUILD_MDW_PLATFORMS_LINK_PATH})
    target_link_directories(${PROJECT_NAME} PRIVATE ${ANJ_FLASH_LINK_DIR})
endforeach()

if(BUILD_PLATFORMS_NAME MATCHES "^mstar")
    set(ANJFLASH_LINK_LIBS mi_sys mi_common cam_os_wrapper)
elseif(BUILD_PLATFORMS_NAME STREQUAL "ts_5326")
    set(ANJFLASH_LINK_LIBS ${BUILD_ANJFLASH_LINK_LIB})
else()
    message(FATAL_ERROR "Unsupported BUILD_PLATFORMS_NAME for anjflash: ${BUILD_PLATFORMS_NAME}")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE m pthread dl ${ANJFLASH_LINK_LIBS})

set(STRIPPED_TARGET "${PROJECT_NAME}_stripped")
add_custom_target(${STRIPPED_TARGET} ALL
    COMMAND ${BUILD_COMPILER}strip $<TARGET_FILE:${PROJECT_NAME}>
    DEPENDS ${PROJECT_NAME}
    COMMENT "Stripping debug symbols from ${PROJECT_NAME}"
)
