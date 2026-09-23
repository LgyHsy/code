cmake_minimum_required (VERSION 3.1.0)

# 设置内核源代码路径
set(KDIR /home/leo/nfs/sdk/kernel_sc377)

# 定义编译函数/方法，驱动编译过程主要在这个函数进行
function(compile_module obj)
  set(TARGET_NAME ${obj})

  # 添加目标，obj即为模块名称。由于我在build目录下构建，所以这里选择将.c和.h文件拷贝到该目录下
  add_custom_target(${TARGET_NAME} ALL cp -f ${CMAKE_CURRENT_SOURCE_DIR}/*.c ${CMAKE_CURRENT_BINARY_DIR}/
			COMMAND cp -f ${CMAKE_CURRENT_SOURCE_DIR}/*.h ${CMAKE_CURRENT_BINARY_DIR}/
      COMMAND cp -f ${CMAKE_CURRENT_SOURCE_DIR}/make_ptz_module.sh ${CMAKE_CURRENT_BINARY_DIR}/
      COMMAND echo "compiling module ${obj}.ko..."
  		)

  # 设置依赖，相当于Makefile中的 $(MODULE_NAME)-objs += demo_main.o
  #aux_source_directory(. SRC_LIST)
  FILE(GLOB SRC_FILES "*.c" "*.cc" "*.cpp")
  message(STATUS "src list=${SRC_FILES}")
  set(depend_objlist "")
  FOREACH(child ${SRC_FILES})
	  string(REGEX REPLACE ".+/(.+)\\..*" "\\1" FILE_NAME ${child})
	  set(depend_objlist "${depend_objlist} ${FILE_NAME}.o")
  ENDFOREACH()
  message(STATUS "depend_objlist = ${depend_objlist}")
  
  # 设置编译命令
  add_custom_command(TARGET ${TARGET_NAME}
		     POST_BUILD
		     COMMAND echo "obj-m := ${obj}.o" > ${CMAKE_CURRENT_BINARY_DIR}/Makefile
		     COMMAND echo "${obj}-objs:=${depend_objlist}" >>${CMAKE_CURRENT_BINARY_DIR}/Makefile
		     COMMAND ${CMAKE_COMMAND} -E echo "MAKE ${CMAKE_CURRENT_BINARY_DIR}" >>${CMAKE_CURRENT_BINARY_DIR}/make_ptz_module.sh
		     COMMAND chmod 777 ${CMAKE_CURRENT_BINARY_DIR}/make_ptz_module.sh		     )
endfunction()

# 调用编译函数
string(REGEX REPLACE ".*/(.*)" "\\1" CURRENT_FOLDER ${CMAKE_CURRENT_SOURCE_DIR})
message(STATUS "test ${CURRENT_FOLDER}")
compile_module(${CURRENT_FOLDER})
