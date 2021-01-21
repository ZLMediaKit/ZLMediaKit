project(ZLMediaKit)
cmake_minimum_required(VERSION 3.1.3)
#使能c++11
set(CMAKE_CXX_STANDARD 11)

#设置生成的so动态库最后输出的路径
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libs_export/${ANDROID_ABI})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libs_export/${ANDROID_ABI})

LINK_DIRECTORIES(${LIBRARY_OUTPUT_PATH})

#设置工程源码根目录
set(ZLMediaKit_Root ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../)
set(JNI_Root ${CMAKE_CURRENT_SOURCE_DIR})
set(ToolKit_Root ${ZLMediaKit_Root}/3rdpart/ZLToolKit/src)
set(MediaKit_Root ${ZLMediaKit_Root}/src)
set(MediaServer_Root ${ZLMediaKit_Root}/3rdpart/media-server/)

#设置头文件目录
INCLUDE_DIRECTORIES(${ToolKit_Root})
INCLUDE_DIRECTORIES(${MediaKit_Root})
INCLUDE_DIRECTORIES(${JNI_Root})

#收集源代码
file(GLOB ToolKit_src_list ${ToolKit_Root}/*/*.cpp ${ToolKit_Root}/*/*.h ${ToolKit_Root}/*/*.c)
file(GLOB MediaKit_src_list ${MediaKit_Root}/*/*.cpp ${MediaKit_Root}/*/*.h ${MediaKit_Root}/*/*.c)
file(GLOB JNI_src_list ${JNI_Root}/*.cpp ${JNI_Root}/*.h)

#去除win32的适配代码
if (NOT WIN32)
    list(REMOVE_ITEM ToolKit_src_list ${ToolKit_Root}/win32/getopt.c)
else()
     #防止Windows.h包含Winsock.h
    add_definitions(-DWIN32_LEAN_AND_MEAN -DMP4V2_NO_STDINT_DEFS)
endif ()

set(ENABLE_HLS true)
set(ENABLE_OPENSSL true)
set(ENABLE_MYSQL false)
set(ENABLE_FAAC false)
set(ENABLE_X264 false)
set(ENABLE_MP4 true)

#添加两个静态库
if(ENABLE_HLS)
    message(STATUS "ENABLE_HLS defined")
    add_definitions(-DENABLE_HLS)
    set(LINK_LIB_LIST zlmediakit zltoolkit mpeg)
else()
    set(LINK_LIB_LIST zlmediakit zltoolkit)
endif()


if(ENABLE_MP4)
    message(STATUS "ENABLE_MP4 defined")
    add_definitions(-DENABLE_MP4)
	list(APPEND LINK_LIB_LIST mov flv)
endif()

if (ENABLE_OPENSSL)
    #openssl
    add_definitions(-DENABLE_OPENSSL)
    INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/../../../libs/${ANDROID_ABI}/include)
    LINK_DIRECTORIES(${CMAKE_SOURCE_DIR}/../../../libs/${ANDROID_ABI}/)
    list(APPEND LINK_LIB_LIST ssl crypto)
endif ()

#libmpeg
if(ENABLE_HLS)
    aux_source_directory(${MediaServer_Root}/libmpeg/include src_mpeg)
    aux_source_directory(${MediaServer_Root}/libmpeg/source src_mpeg)
    include_directories(${MediaServer_Root}/libmpeg/include)
    add_library(mpeg STATIC ${src_mpeg})
	if(WIN32)
		set_target_properties(mpeg PROPERTIES COMPILE_FLAGS  ${VS_FALGS} )
	endif(WIN32)
endif()

if(ENABLE_MP4)
    aux_source_directory(${MediaServer_Root}/libmov/include src_mov)
    aux_source_directory(${MediaServer_Root}/libmov/source src_mov)
	include_directories(${MediaServer_Root}/libmov/include)
    aux_source_directory(${MediaServer_Root}/libflv/include src_flv)
    aux_source_directory(${MediaServer_Root}/libflv/source src_flv)
	include_directories(${MediaServer_Root}/libflv/include)
	add_library(mov STATIC ${src_mov})
	add_library(flv STATIC ${src_flv})
	if(WIN32)
		set_target_properties(mov flv PROPERTIES COMPILE_FLAGS  ${VS_FALGS} )
	endif(WIN32)
endif()


#添加库
add_library(zltoolkit STATIC ${ToolKit_src_list})
add_library(zlmediakit STATIC ${MediaKit_src_list})
add_library(zlmediakit_jni SHARED ${JNI_src_list})

add_definitions(-DDISABLE_MAIN)
#添加json以及MediaServer相关代码
include_directories(${ZLMediaKit_Root}/3rdpart)
include_directories(${ZLMediaKit_Root}/server)
file(GLOB jsoncpp_src_list ${ZLMediaKit_Root}/3rdpart/jsoncpp/*.cpp)
file(GLOB MediaServer_src_list ${ZLMediaKit_Root}/server/*.cpp)
add_library(jsoncpp STATIC ${jsoncpp_src_list})
add_library(MediaServer STATIC ${MediaServer_src_list})

#链接
target_link_libraries(zlmediakit_jni MediaServer jsoncpp ${LINK_LIB_LIST} log z)


