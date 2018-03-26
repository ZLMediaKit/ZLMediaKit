project(ZLMediaKit)
cmake_minimum_required(VERSION 3.1.3)
#使能c++11
set(CMAKE_CXX_STANDARD 11)

#加载自定义模块
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")
#设置库文件路径
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
#设置可执行程序路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

#设置工程源码根目录
set(ToolKit_Root ${CMAKE_SOURCE_DIR}/ZLToolKit/src)
set(MediaKit_Root ${CMAKE_SOURCE_DIR}/src)

#设置头文件目录
INCLUDE_DIRECTORIES(${ToolKit_Root})
INCLUDE_DIRECTORIES(${MediaKit_Root})

#收集源代码
file(GLOB ToolKit_src_list ${ToolKit_Root}/*/*.cpp ${ToolKit_Root}/*/*.h ${ToolKit_Root}/*/*.c)
file(GLOB MediaKit_src_list ${MediaKit_Root}/*/*.cpp ${MediaKit_Root}/*/*.h ${MediaKit_Root}/*/*.c)

#去除win32的适配代码
if (NOT WIN32)
    list(REMOVE_ITEM ToolKit_src_list ${ToolKit_Root}/win32/getopt.c)
endif ()

#添加两个静态库
set(LINK_LIB_LIST zlmediakit zltoolkit)

#查找openssl是否安装
find_package(OpenSSL QUIET)
if (OPENSSL_FOUND)
    message(STATUS "found openssl library\"${OPENSSL_INCLUDE_DIR}\",ENABLE_OPENSSL enabled")
    include_directories(${OPENSSL_INCLUDE_DIR})
    add_definitions(-DENABLE_OPENSSL)
    list(APPEND LINK_LIB_LIST ${OPENSSL_LIBRARIES})
endif (OPENSSL_FOUND)

#查找mysql是否安装
find_package(MYSQL QUIET)
if (MYSQL_FOUND)
    message(STATUS "found mysqlclient library\"${MYSQL_INCLUDE_DIR}\",ENABLE_MYSQL enabled")
    include_directories(${MYSQL_INCLUDE_DIR})
    add_definitions(-DENABLE_MYSQL)
    list(APPEND LINK_LIB_LIST ${MYSQL_LIBRARIES})
endif (MYSQL_FOUND)

#查找MP4V2是否安装
find_package(MP4V2 QUIET)
if (MP4V2_FOUND)
    include_directories(${MP4V2_INCLUDE_DIR})
    list(APPEND LINK_LIB_LIST ${MP4V2_LIBRARY})
    add_definitions(-DENABLE_MP4V2)
    message(STATUS "found MP4V2:${MP4V2_INCLUDE_DIR},${MP4V2_LIBRARY}")
endif (MP4V2_FOUND)

#查找x264是否安装
find_package(X264 QUIET)
if (X264_FOUND)
    message(STATUS "found x264 library\"${X264_INCLUDE_DIRS}\",ENABLE_X264 enabled")
    include_directories(${X264_INCLUDE_DIRS})
    add_definitions(-DENABLE_X264)
    list(APPEND LINK_LIB_LIST ${X264_LIBRARIES})
endif ()

#查找faac是否安装
find_package(FAAC QUIET)
if (FAAC_FOUND)
    message(STATUS "found faac library\"${FAAC_INCLUDE_DIR}\",ENABLE_FAAC enabled")
    include_directories(${FAAC_INCLUDE_DIR})
    add_definitions(-DENABLE_FAAC)
    list(APPEND LINK_LIB_LIST ${FAAC_LIBRARIES})
endif ()

#使能GOP缓存
add_definitions(-DENABLE_RING_USEBUF)
#添加库
add_library(zltoolkit STATIC ${ToolKit_src_list})
add_library(zlmediakit STATIC ${MediaKit_src_list})

if (WIN32)
    list(APPEND LINK_LIB_LIST WS2_32 Iphlpapi shlwapi)
elseif(NOT ANDROID OR IOS)
    list(APPEND LINK_LIB_LIST pthread)
endif ()

message(STATUS "linked libraries:${LINK_LIB_LIST}")

#测试程序
add_subdirectory(tests)



























