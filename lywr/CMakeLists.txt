#定义工程名及版本
project(lywr)
set(version 1.0.1)
set(soversion 1.0)

cmake_minimum_required(VERSION 3.0)
set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON)
set(CMAKE_USE_RELATIVE_PATHS ON)
set(CMAKE_INCLUDE_DIRECTORIES_BEFORE ON)
set(CMAKE_BUILE_TYPE DEBUG) 

#导入一此需要用到的工具包
include(CheckTypeSize)
include(CheckFunctionExists)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckCSourceCompiles)

#定制编译选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall --pedantic -fvisibility=hidden -Wno-unused-function -Wno-error=switch")
set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} -Wall")

if(release)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O4 -DNDEBUG -Wall ")
	set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} -O4 -Wall ")
	message("Build as release")
else()
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_DEBUG -g -Wall -Werror -pedantic -fstack-protector ")
	set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} -g -Wall -Werror -pedantic -fstack-protector ")
	message("Build as debug")
endif()

#定义安装目录
if(NOT install_dir)
	set(install_dir ${CMAKE_CURRENT_SOURCE_DIR}/output)
endif()

#搜索源代码
aux_source_directory(src src_c)

#搜索头文件
file(GLOB src_hs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.h)
include_directories(src)

#目标动态库
add_library(${PROJECT_NAME} SHARED ${src_c} ${src_hs})
set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS -nostdlib)
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS -nostdlib)


#定义安装目标
install(TARGETS ${PROJECT_NAME} DESTINATION ${install_dir})
#拷贝文件
file(COPY ${src_hs} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/)



#定义应用目标
file(GLOB src_app RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/app/*.c)
foreach(var ${src_app})
	string(REGEX REPLACE "src/app/(.*).c" "\\1" var2 ${var})
	add_executable(${var2} ${var})
	target_link_libraries(${var2} ${PROJECT_NAME})
	install(TARGETS ${PROJECT_NAME} DESTINATION ${install_dir})
endforeach()
