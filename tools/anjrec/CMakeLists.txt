cmake_minimum_required(VERSION 3.10)
project(anjrec C)

set(REPO_ROOT ${CMAKE_SOURCE_DIR})

include(${CMAKE_CURRENT_SOURCE_DIR}/anjrec_defines.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/anjrec_sources.cmake)

string(TIMESTAMP ANJREC_BUILD_TIME "%Y-%m-%d %H:%M:%S")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/anjrec_version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/anjrec_version.h
    @ONLY
)
list(APPEND ANJREC_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out/bin)

add_executable(${PROJECT_NAME} ${ANJREC_ALL_SRC})

target_compile_options(${PROJECT_NAME} PRIVATE ${ANJREC_OPT_FLAGS})
target_compile_definitions(${PROJECT_NAME} PRIVATE ${ANJREC_COMPILE_DEFS})
target_compile_options(${PROJECT_NAME} PRIVATE -ffunction-sections -fdata-sections)

target_include_directories(${PROJECT_NAME} PRIVATE ${ANJREC_INCLUDE_DIRS})

foreach(ANJREC_LINK_DIR ${ANJREC_LINK_DIRS})
    target_link_directories(${PROJECT_NAME} PRIVATE ${ANJREC_LINK_DIR})
endforeach()

target_link_options(${PROJECT_NAME} PRIVATE
    -Wl,--gc-sections
    -Wl,--wrap=anj_mw_system_with_param
    ${ANJREC_LINK_KEEP_FLAGS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${ANJREC_LINK_LIBS})

if(NOT DEBUG)
    set(STRIPPED_TARGET "${PROJECT_NAME}_stripped")
    add_custom_target(${STRIPPED_TARGET} ALL
        COMMAND ${BUILD_COMPILER}strip $<TARGET_FILE:${PROJECT_NAME}>
        DEPENDS ${PROJECT_NAME}
        COMMENT "Stripping debug symbols from ${PROJECT_NAME}"
    )
endif()

message(STATUS "anjrec: project=${BUILD_PROJECT_NAME} platform=${BUILD_PLATFORMS_NAME}")
