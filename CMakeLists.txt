cmake_minimum_required(VERSION 3.10)

add_custom_target(config ALL DEPENDS ${CMAKE_BINARY_DIR}/.config)
option(ANJ_SKIP_APP "Skip building app/anjcam targets" OFF)

execute_process(COMMAND python3 ${CMAKE_SOURCE_DIR}/parse_config.py)
include(${CMAKE_SOURCE_DIR}/config.cmake)

# 导出打包清单：link_catalog.conf + feature_config.cmake
set(_ANJ_MANIFEST_DIR "${CMAKE_SOURCE_DIR}/out/manifest")
set(_ANJ_LINK_CATALOG "${CMAKE_SOURCE_DIR}/module/anj_link_catalog.conf")
file(MAKE_DIRECTORY "${_ANJ_MANIFEST_DIR}")
if(EXISTS "${_ANJ_LINK_CATALOG}")
  configure_file("${_ANJ_LINK_CATALOG}" "${_ANJ_MANIFEST_DIR}/link_catalog.conf" COPYONLY)
  set(_ANJ_FEATURE_CONTENT "# 改 y/n 即可，不要删行\n")
  file(STRINGS "${_ANJ_LINK_CATALOG}" _ANJ_CATALOG_LINES)
  foreach(_ANJ_LINE IN LISTS _ANJ_CATALOG_LINES)
    string(REGEX REPLACE "#.*" "" _ANJ_LINE "${_ANJ_LINE}")
    string(STRIP "${_ANJ_LINE}" _ANJ_LINE)
    if(_ANJ_LINE MATCHES "^flag\\|([^|]+)\\|([^|]+)")
      set(_ANJ_CMAKE_FLAG "${CMAKE_MATCH_2}")
      if(DEFINED ${_ANJ_CMAKE_FLAG})
        set(_ANJ_VAL "${${_ANJ_CMAKE_FLAG}}")
      else()
        set(_ANJ_VAL "n")
      endif()
      string(APPEND _ANJ_FEATURE_CONTENT "set(${_ANJ_CMAKE_FLAG} ${_ANJ_VAL})\n")
    endif()
  endforeach()
  file(WRITE "${_ANJ_MANIFEST_DIR}/feature_config.cmake" "${_ANJ_FEATURE_CONTENT}")
endif()

### 设备编译链
set(CMAKE_C_COMPILER ${BUILD_COMPILER}gcc)
set(CMAKE_CXX_COMPILER ${BUILD_COMPILER}g++)

PROJECT(anjcam)

# 设置指定的C++编译器版本是必须的，如果不设置，或者为OFF，则指定版本不可用时，会使用上一版本。
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 指定为C++11 版本
set(CMAKE_CXX_STANDARD 11)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/out/bin/)

if(DEBUG)
add_definitions("-Wall -Wno-psabi -g -O0")
else()
add_definitions("-Wall -Wno-psabi -O3")
endif()

if(PQTOOL)
  add_definitions("-D_SUPPORT_IQTOOL_=1")
else()
  add_definitions("-D_SUPPORT_IQTOOL_=0")
endif()

if(BUILD_PROJECT_NAME)
    string(TOUPPER "${BUILD_PROJECT_NAME}" PROJECT_MACRO_NAME)
    add_definitions(-D${PROJECT_MACRO_NAME})
    message(STATUS "Project macro defined: ${PROJECT_MACRO_NAME}")
endif()

if(BUILD_PLATFORMS_NAME)
    string(TOUPPER "${BUILD_PLATFORMS_NAME}" PLATFORM_MACRO_NAME)
    add_definitions(-D${PLATFORM_MACRO_NAME})
    message(STATUS "Platform macro defined: ${PLATFORM_MACRO_NAME}")
endif()

include (project/project_option.cmake)

add_definitions("-lm -lpthread")

include_directories(${BUILD_PROJECT_TARGET_PATH}/${BUILD_INC})
###编译middleware
add_subdirectory(${BUILD_MDW_PATH})

###编译daemon
add_subdirectory(${BUILD_DAEMON_PATH})

###编译 anj_flash
add_subdirectory(${BUILD_ANJ_FLASH_PATH})

###编译 anjrec
add_subdirectory(${BUILD_ANJ_REC_PATH})

###编译app
add_subdirectory(${BUILD_APP_PATH})

### 添加链接路径
foreach (MDW_PLATFORMS_LINK_DIR_ ${BUILD_MDW_PLATFORMS_LINK_PATH})
    message(STATUS "BUILD_MDW_PLATFORMS_LINK_PATH=" ${MDW_PLATFORMS_LINK_DIR_})
    link_directories(${MDW_PLATFORMS_LINK_DIR_})
endforeach()

foreach (MODULE_LINK_DIR_LIST_ ${MODULE_LINK_DIR_LIST})
    message(STATUS "MODULE_LINK_DIR_LIST=" ${MODULE_LINK_DIR_LIST_})
    link_directories(${MODULE_LINK_DIR_LIST_})
endforeach()

if(NOT ANJ_SKIP_APP)
  file(WRITE ${CMAKE_BINARY_DIR}/anjcam_dummy.c "void anjcam_link_anchor(void) {}\n")
  add_executable(${PROJECT_NAME} ${CMAKE_BINARY_DIR}/anjcam_dummy.c)

  set(ANJ_LINK_STATIC_TARGETS anjapp)
  foreach(ANJ_STATIC_TARGET_ ${ANJ_MODULE_STATIC_TARGETS})
    if(TARGET ${ANJ_STATIC_TARGET_})
      list(APPEND ANJ_LINK_STATIC_TARGETS ${ANJ_STATIC_TARGET_})
      message(STATUS "ANJ_LINK_STATIC_TARGETS=" ${ANJ_STATIC_TARGET_})
    endif()
  endforeach()

  set(ANJ_LINK_KEEP_FLAGS "")
  foreach(ANJ_LINK_KEEP_SYMBOL_ ${ANJ_LINK_KEEP_SYMBOLS})
    list(APPEND ANJ_LINK_KEEP_FLAGS "-Wl,--undefined=${ANJ_LINK_KEEP_SYMBOL_}")
  endforeach()

  add_dependencies(${PROJECT_NAME} middleware anjapp anj_module_common_libs)

  target_link_libraries(${PROJECT_NAME} PRIVATE
    ${ANJ_LINK_KEEP_FLAGS}
    -Wl,--start-group
    ${ANJ_LINK_STATIC_TARGETS}
    middleware
    ${MODULE_LINK_LIBS}
    -Wl,--end-group
    stdc++
    m
    pthread
  )

  ###strip
  if (NOT DEBUG)
      set(STRIPPED_TARGET "${PROJECT_NAME}_stripped")
      # 添加自定义目标来strip可执行文件
      add_custom_target(${STRIPPED_TARGET} ALL
          COMMAND ${BUILD_COMPILER}strip $<TARGET_FILE:${PROJECT_NAME}>
          DEPENDS ${PROJECT_NAME}
          COMMENT "Stripping debug symbols from ${PROJECT_NAME}"
      )
  endif()
endif()
