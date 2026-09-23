cmake_minimum_required(VERSION 3.10)

project(softsn LANGUAGES C)

include(${CMAKE_SOURCE_DIR}/../config.cmake)

# 保持和主CMakeLists.txt相同的配置
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-Wall -g -O0)
else()
    add_compile_options(-Wall -O3)
endif()

# 添加头文件目录
if(NOT DEFINED BUILD_PLATFORMS_NAME)
    set(BUILD_PLATFORMS_NAME "mstar_37x")
endif()
if(NOT DEFINED BUILD_PROJECT_NAME)
    set(BUILD_PROJECT_NAME "MYF30")
endif()

set(PLATFORM_INC_DIR ${PROJECT_ROOT}/middleware/platforms/${BUILD_PLATFORMS_NAME}/inc)
set(PROJECT_INC_DIR ${PROJECT_ROOT}/project/${BUILD_PROJECT_NAME}/inc)
set(SN_PLATFORM_DIR ${CMAKE_SOURCE_DIR}/platform/${BUILD_PLATFORMS_NAME})

include_directories(
    ${CMAKE_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/platform/${BUILD_PLATFORMS_NAME}/inc
    ${CMAKE_SOURCE_DIR}/src/encrypt
    ${CMAKE_SOURCE_DIR}/..${PLATFORM_INC_DIR}
    ${CMAKE_SOURCE_DIR}/..${PROJECT_INC_DIR}
    ${CMAKE_SOURCE_DIR}/../module/comm/inc
    ${CMAKE_SOURCE_DIR}/../module/config/inc
    ${CMAKE_SOURCE_DIR}/../module/service/anjser/inc
    ${CMAKE_SOURCE_DIR}/../module/sysmng/inc
    ${CMAKE_SOURCE_DIR}/../middleware/comm/inc
)

if(BUILD_PROJECT_SDK_PATH)
    include_directories(${BUILD_PROJECT_SDK_PATH}/include)
endif()

# 查找源文件
file(GLOB COMMON_SOURCES "src/*.c" "src/encrypt/*.c")
file(GLOB PLATFORM_SOURCES "${CMAKE_SOURCE_DIR}/platform/${BUILD_PLATFORMS_NAME}/src/*.c")
set(ALL_SOURCES ${COMMON_SOURCES} ${PLATFORM_SOURCES})

if(NOT ALL_SOURCES)
    message(FATAL_ERROR "No source files found in src directory!")
endif()

# 创建库
option(BUILD_SHARED_LIBS "Build shared library" OFF)

if(BUILD_SHARED_LIBS)
    add_library(softsn SHARED ${ALL_SOURCES})
    set_target_properties(softsn PROPERTIES
        OUTPUT_NAME "softsn"
    )
else()
    add_library(softsn STATIC ${ALL_SOURCES})
    set_target_properties(softsn PROPERTIES
        OUTPUT_NAME "softsn"
    )
endif()