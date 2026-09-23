# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
PROJECT(anjdaemon)

# 设置指定的C++编译器版本是必须的，如果不设置，或者为OFF，则指定版本不可用时，会使用上一版本。
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 指定为C++11 版本
set(CMAKE_CXX_STANDARD 11)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/../out/bin/)

###添加源文件
aux_source_directory(${BUILD_DAEMON_PATH}/${BUILD_SRC} SRC_LIST)

add_executable(${PROJECT_NAME} ${SRC_LIST})

###strip
set(STRIPPED_TARGET "${PROJECT_NAME}_stripped")
# 添加自定义目标来strip可执行文件
add_custom_target(${STRIPPED_TARGET} ALL
    COMMAND ${BUILD_COMPILER}strip $<TARGET_FILE:${PROJECT_NAME}>
    DEPENDS ${PROJECT_NAME}
    COMMENT "Stripping debug symbols from ${PROJECT_NAME}"
)