project(ZLMediaKit)
cmake_minimum_required(VERSION 3.1.3)
#使能c++11
set(CMAKE_CXX_STANDARD 11)
#加载自定义模块
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

#set(CMAKE_BUILD_TYPE "Release")

if(${CMAKE_BUILD_TYPE} MATCHES "Release")
    message(STATUS "Release版本")
    set(BuildType "Release")
else()
    set(BuildType "Debug")
    message(STATUS "Debug版本")
endif()

#设置bin和lib库目录
set(RELEASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/release)
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    SET(LIBRARY_OUTPUT_PATH ${RELEASE_DIR}/linux/${BuildType})
    SET(EXECUTABLE_OUTPUT_PATH ${RELEASE_DIR}/linux/${BuildType})
    add_compile_options(-fPIC)
elseif (CMAKE_SYSTEM_NAME MATCHES "Windows")
    SET(LIBRARY_OUTPUT_PATH ${RELEASE_DIR}/windows/${BuildType})
    SET(EXECUTABLE_OUTPUT_PATH ${RELEASE_DIR}/windows/${BuildType})
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    SET(LIBRARY_OUTPUT_PATH ${RELEASE_DIR}/mac/${BuildType})
    SET(EXECUTABLE_OUTPUT_PATH ${RELEASE_DIR}/mac/${BuildType})
endif ()

LINK_DIRECTORIES(${LIBRARY_OUTPUT_PATH})

#设置工程源码根目录
set(ToolKit_Root ${CMAKE_CURRENT_SOURCE_DIR}/3rdpart/ZLToolKit/src)
set(MediaKit_Root ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(MediaServer_Root ${CMAKE_CURRENT_SOURCE_DIR}/3rdpart/media-server)

#设置头文件目录
INCLUDE_DIRECTORIES(${ToolKit_Root})
INCLUDE_DIRECTORIES(${MediaKit_Root})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/3rdpart)

option(ENABLE_HLS "Enable HLS" true)
option(ENABLE_OPENSSL "Enable OpenSSL" true)
option(ENABLE_MYSQL "Enable MySQL" false)
option(ENABLE_FAAC "Enable FAAC" false)
option(ENABLE_X264 "Enable x264" false)
option(ENABLE_MP4 "Enable MP4" true)
option(ENABLE_RTPPROXY "Enable RTPPROXY" true)
option(ENABLE_API "Enable C API SDK" true)
option(ENABLE_CXX_API "Enable C++ API SDK" false)
option(ENABLE_TESTS "Enable Tests" true)
option(ENABLE_SERVER "Enable Server" true)

set(LINK_LIB_LIST zlmediakit zltoolkit)

#查找openssl是否安装
find_package(OpenSSL QUIET)
if (OPENSSL_FOUND AND ENABLE_OPENSSL)
    message(STATUS "found library:${OPENSSL_LIBRARIES},ENABLE_OPENSSL defined")
    include_directories(${OPENSSL_INCLUDE_DIR})
    add_definitions(-DENABLE_OPENSSL)
    list(APPEND LINK_LIB_LIST ${OPENSSL_LIBRARIES})
else()
    message(WARNING "openssl未找到，rtmp将不支持flash播放器，https/wss/rtsps/rtmps也将失效")
endif ()

#查找mysql是否安装
find_package(MYSQL QUIET)
if (MYSQL_FOUND AND ENABLE_MYSQL)
    message(STATUS "found library:${MYSQL_LIBRARIES},ENABLE_MYSQL defined")
    include_directories(${MYSQL_INCLUDE_DIR})
    include_directories(${MYSQL_INCLUDE_DIR}/mysql)
    add_definitions(-DENABLE_MYSQL)
    list(APPEND LINK_LIB_LIST ${MYSQL_LIBRARIES})
endif ()

#查找x264是否安装
find_package(X264 QUIET)
if (X264_FOUND AND ENABLE_X264)
    message(STATUS "found library:${X264_LIBRARIES},ENABLE_X264 defined")
    include_directories(${X264_INCLUDE_DIRS})
    add_definitions(-DENABLE_X264)
    list(APPEND LINK_LIB_LIST ${X264_LIBRARIES})
endif ()

#查找faac是否安装
find_package(FAAC QUIET)
if (FAAC_FOUND AND ENABLE_FAAC)
    message(STATUS "found library:${FAAC_LIBRARIES},ENABLE_FAAC defined")
    include_directories(${FAAC_INCLUDE_DIR})
    add_definitions(-DENABLE_FAAC)
    list(APPEND LINK_LIB_LIST ${FAAC_LIBRARIES})
endif ()

if(${CMAKE_BUILD_TYPE} MATCHES "Release")
    #查找jemalloc是否安装
    find_package(JEMALLOC QUIET)
    if(JEMALLOC_FOUND)
        message(STATUS "found library:\"${JEMALLOC_LIBRARIES}\"")
        include_directories(${JEMALLOC_INCLUDE_DIR})
        list(APPEND  LINK_LIB_LIST ${JEMALLOC_LIBRARIES})
    endif()
endif()

set(VS_FALGS "/wd4819 /wd4996 /wd4018 /wd4267 /wd4244 /wd4101 /wd4828 /wd4309 /wd4573" )

#添加mpeg用于支持ts生成
if(ENABLE_HLS)
    message(STATUS "ENABLE_HLS defined")
    add_definitions(-DENABLE_HLS)

    include_directories(${MediaServer_Root}/libmpeg/include)
    aux_source_directory(${MediaServer_Root}/libmpeg/include src_mpeg)
    aux_source_directory(${MediaServer_Root}/libmpeg/source src_mpeg)

    add_library(mpeg STATIC ${src_mpeg})
    list(APPEND LINK_LIB_LIST mpeg)
    list(APPEND CXX_API_TARGETS mpeg)

	if(WIN32)
		set_target_properties(mpeg PROPERTIES COMPILE_FLAGS  ${VS_FALGS} )
	endif(WIN32)
endif()

#添加mov、flv库用于MP4录制
if(ENABLE_MP4)
    message(STATUS "ENABLE_MP4 defined")
    add_definitions(-DENABLE_MP4)

    include_directories(${MediaServer_Root}/libmov/include)
    include_directories(${MediaServer_Root}/libflv/include)

    aux_source_directory(${MediaServer_Root}/libmov/include src_mov)
    aux_source_directory(${MediaServer_Root}/libmov/source src_mov)	

    aux_source_directory(${MediaServer_Root}/libflv/include src_flv)
    aux_source_directory(${MediaServer_Root}/libflv/source src_flv)	

    add_library(mov STATIC ${src_mov})
	add_library(flv STATIC ${src_flv})
    list(APPEND LINK_LIB_LIST mov flv)
    list(APPEND CXX_API_TARGETS mov flv)

	if(WIN32)
		set_target_properties(mov flv PROPERTIES COMPILE_FLAGS  ${VS_FALGS} )
	endif(WIN32)
endif()

#添加rtp库用于rtp转ps/ts
if(ENABLE_RTPPROXY AND ENABLE_HLS)
    message(STATUS "ENABLE_RTPPROXY defined")
    include_directories(${MediaServer_Root}/librtp/include)

    aux_source_directory(${MediaServer_Root}/librtp/include src_rtp)
    aux_source_directory(${MediaServer_Root}/librtp/source src_rtp)
    aux_source_directory(${MediaServer_Root}/librtp/payload src_rtp)
    add_library(rtp STATIC ${src_rtp})
    add_definitions(-DENABLE_RTPPROXY)
    list(APPEND LINK_LIB_LIST rtp)
    list(APPEND CXX_API_TARGETS rtp)
endif()

#收集源代码
file(GLOB ToolKit_src_list ${ToolKit_Root}/*/*.cpp ${ToolKit_Root}/*/*.h ${ToolKit_Root}/*/*.c)
if(IOS)
    list(APPEND ToolKit_src_list ${ToolKit_Root}/Network/Socket_ios.mm)
endif()

file(GLOB MediaKit_src_list ${MediaKit_Root}/*/*.cpp ${MediaKit_Root}/*/*.h ${MediaKit_Root}/*/*.c)

#去除win32的适配代码
if (NOT WIN32)
    list(REMOVE_ITEM ToolKit_src_list ${ToolKit_Root}/win32/getopt.c)
else()
    #防止Windows.h包含Winsock.h
    add_definitions(-DWIN32_LEAN_AND_MEAN -DMP4V2_NO_STDINT_DEFS -DOS_WINDOWS)
endif ()

#添加库
add_library(zltoolkit STATIC ${ToolKit_src_list})
add_library(zlmediakit STATIC ${MediaKit_src_list})
list(APPEND CXX_API_TARGETS zltoolkit zlmediakit)

#安装目录
if (WIN32)
    set(INSTALL_PATH_LIB $ENV{HOME}/${CMAKE_PROJECT_NAME}/lib)
    set(INSTALL_PATH_INCLUDE $ENV{HOME}/${CMAKE_PROJECT_NAME}/include)
else ()
    set(INSTALL_PATH_LIB lib)
    set(INSTALL_PATH_INCLUDE include)
endif ()

if(ENABLE_CXX_API)
    # 保留目录结构
    install(DIRECTORY ${ToolKit_Root}/  DESTINATION ${INSTALL_PATH_INCLUDE}/ZLToolKit  REGEX "(.*[.](md|cpp)|win32)$" EXCLUDE)
    install(DIRECTORY ${MediaKit_Root}/ DESTINATION ${INSTALL_PATH_INCLUDE}/ZLMediaKit REGEX ".*[.](md|cpp)$" EXCLUDE)
    install(TARGETS ${CXX_API_TARGETS} DESTINATION ${INSTALL_PATH_LIB})
endif()

if (WIN32)
    list(APPEND LINK_LIB_LIST WS2_32 Iphlpapi shlwapi)
	set_target_properties(zltoolkit PROPERTIES COMPILE_FLAGS ${VS_FALGS} )
	set_target_properties(zlmediakit PROPERTIES COMPILE_FLAGS ${VS_FALGS} )
elseif(NOT ANDROID OR IOS)
    list(APPEND LINK_LIB_LIST pthread)
endif ()

#复制文件过来
execute_process(COMMAND cp -r ${CMAKE_CURRENT_SOURCE_DIR}/www ${EXECUTABLE_OUTPUT_PATH}/)
execute_process(COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/conf/config.ini ${EXECUTABLE_OUTPUT_PATH}/)

#添加c库
if(ENABLE_API)
    add_subdirectory(api)
endif()

if (NOT IOS)
    #测试程序
    if(ENABLE_TESTS)
        add_subdirectory(tests)
    endif()
    #主服务器
    if(ENABLE_SERVER)
        add_subdirectory(server)
    endif()
endif ()
