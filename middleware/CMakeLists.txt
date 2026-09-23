cmake_minimum_required(VERSION 3.10)
# 设置 库文件的默认输出路径
string(REGEX REPLACE ".*/(.*)" "\\1" TARGET_NAME ${CMAKE_CURRENT_SOURCE_DIR})
set (LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/out/lib/)
set(ANJ_MDW_PLATFORM_LIB_OUTPUT_DIR ${PROJECT_SOURCE_DIR}/out/lib/platforms)

add_definitions("-Werror")

list(APPEND BUILD_MDW_SRC_LIST ${BUILD_MDW_PLATFORMS_SRC_PATH})
list(APPEND BUILD_MDW_INC_LIST ${BUILD_MDW_PLATFORMS_INC_PATH})
list(APPEND BUILD_MDW_LINK_LIST ${BUILD_MDW_PLATFORMS_LINK_PATH})
list(APPEND BUILD_MDW_LIB_LIST ${BUILD_MDW_PLATFORMS_LINK_LIB})

function(anj_middleware_collect_platform_libs out_var)
    set(platform_lib_paths)

    if(SDK_LIBS_STATIC)
        set(platform_lib_suffix .a)
    else()
        set(platform_lib_suffix .so)
    endif()

    foreach(platform_lib ${BUILD_MDW_PLATFORMS_LINK_LIB})
        foreach(platform_lib_dir ${BUILD_MDW_PLATFORMS_LINK_PATH})
            set(platform_lib_path ${platform_lib_dir}/lib${platform_lib}${platform_lib_suffix})
            if(EXISTS ${platform_lib_path})
                list(APPEND platform_lib_paths ${platform_lib_path})
                break()
            endif()
        endforeach()
    endforeach()

    list(REMOVE_DUPLICATES platform_lib_paths)
    set(${out_var} ${platform_lib_paths} PARENT_SCOPE)
endfunction()

######################### 添加源文件
foreach (BUILD_MDW_SRC_LIST_ ${BUILD_MDW_SRC_LIST})
    message(STATUS "BUILD_MDW_SRC_LIST=" ${BUILD_MDW_SRC_LIST_})
    aux_source_directory(${BUILD_MDW_SRC_LIST_} SRC_LIST)
endforeach()

#########################添加link目录
foreach (BUILD_MDW_LINK_LIST_ ${BUILD_MDW_LINK_LIST})
    message(STATUS "BUILD_MDW_LINK_LIST=" ${BUILD_MDW_LINK_LIST_})
    link_directories(${BUILD_MDW_LINK_LIST_})
endforeach()

#add_library (MDW SHARED ${SRC_LIST})
add_library (${TARGET_NAME} STATIC ${SRC_LIST})
#add_library (${TARGET_NAME}_static STATIC ${SRC_LIST})
#add_library (${TARGET_NAME}_shared SHARED ${SRC_LIST})
# 统一输出基名
#set_target_properties(${TARGET_NAME}_static PROPERTIES OUTPUT_NAME ${TARGET_NAME})
#set_target_properties(${TARGET_NAME}_shared PROPERTIES OUTPUT_NAME ${TARGET_NAME})

######################### 添加头文件
foreach (BUILD_MDW_INC_LIST_ ${BUILD_MDW_INC_LIST})
    message(STATUS "BUILD_MDW_INC_LIST=" ${BUILD_MDW_INC_LIST_})
    target_include_directories(${TARGET_NAME} PRIVATE ${BUILD_MDW_INC_LIST_})
endforeach()

#########################添加link lib
foreach (BUILD_MDW_LIB_LIST_ ${BUILD_MDW_LIB_LIST})
    message(STATUS "BUILD_MDW_LIB_LIST =" ${BUILD_MDW_LIB_LIST_})
    target_link_libraries(${TARGET_NAME} PUBLIC ${BUILD_MDW_LIB_LIST_})
endforeach()

anj_middleware_collect_platform_libs(ANJ_MDW_PLATFORM_LIBS_TO_COPY)
if(ANJ_MDW_PLATFORM_LIBS_TO_COPY)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${ANJ_MDW_PLATFORM_LIB_OUTPUT_DIR}
        VERBATIM
    )

    foreach(platform_lib_path ${ANJ_MDW_PLATFORM_LIBS_TO_COPY})
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${platform_lib_path} ${ANJ_MDW_PLATFORM_LIB_OUTPUT_DIR}/
            VERBATIM
        )
    endforeach()
endif()
