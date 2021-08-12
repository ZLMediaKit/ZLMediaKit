project(ZLMediaKit)
cmake_minimum_required(VERSION 3.1.3)
#使能c++11
set(CMAKE_CXX_STANDARD 11)
#加载自定义模块
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

#set(CMAKE_BUILD_TYPE "Release")
message(STATUS "编译类型:${CMAKE_BUILD_TYPE}")

#设置bin和lib库目录
set(RELEASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/release)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    SET(LIBRARY_OUTPUT_PATH ${RELEASE_DIR}/linux/${CMAKE_BUILD_TYPE})
    SET(EXECUTABLE_OUTPUT_PATH ${RELEASE_DIR}/linux/${CMAKE_BUILD_TYPE})
    add_compile_options(-fPIC -Wall -Wno-unused-variable -Wno-unused-value)
elseif (CMAKE_SYSTEM_NAME MATCHES "Windows")
    if (CMAKE_CL_64)
        set(CL_32_64 64)
    else ()
        set(CL_32_64 32)
    endif ()
    SET(LIBRARY_OUTPUT_PATH ${RELEASE_DIR}/windows${CL_32_64}/${CMAKE_BUILD_TYPE})
    SET(EXECUTABLE_OUTPUT_PATH ${RELEASE_DIR}/windows${CL_32_64}/${CMAKE_BUILD_TYPE})
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    SET(LIBRARY_OUTPUT_PATH ${RELEASE_DIR}/mac/${CMAKE_BUILD_TYPE})
    SET(EXECUTABLE_OUTPUT_PATH ${RELEASE_DIR}/mac/${CMAKE_BUILD_TYPE})
    add_compile_options(-Wall -Wno-unused-variable -Wno-unused-value)
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
option(ENABLE_MEM_DEBUG "Enable Memory Debug" false)
option(ENABLE_ASAN "Enable Address Sanitize" false)
option(ENABLE_WEBRTC "Enable WebRTC" false)
option(ENABLE_PLAYER "Enable Player" true)
option(ENABLE_MSVC_MT "Enable MSVC Mt/Mtd lib" true)

if (MSVC AND ENABLE_MSVC_MT)
    set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        )
    foreach(CompilerFlag ${CompilerFlags})
        string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()
endif()

# 添加git版本信息
set(COMMIT_HASH "Git_NotFound_Unkown_commit")
set(BRANCH_NAME "Git_NotFound_Unkown_branch")
set(BUILD_TIME "")

string(TIMESTAMP BUILD_TIME "%Y/%m/%d-%H:%M:%S")

find_package(Git QUIET)
if (GIT_FOUND)
    execute_process(
            COMMAND ${GIT_EXECUTABLE} log -1 --pretty=format:%h
            OUTPUT_VARIABLE COMMIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    execute_process(
            COMMAND ${GIT_EXECUTABLE} symbolic-ref --short -q HEAD
            OUTPUT_VARIABLE BRANCH_NAME
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif ()

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/version.h.ini
        ${CMAKE_BINARY_DIR}/Version.h
        @ONLY
)

add_definitions(-DENABLE_VERSION)
include_directories(${CMAKE_BINARY_DIR})
message(STATUS "Git version is ${BRANCH_NAME}:${COMMIT_HASH}:${BUILD_TIME}")

if (ENABLE_MEM_DEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-wrap,free  -Wl,-wrap,malloc -Wl,-wrap,realloc -Wl,-wrap,calloc")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -Wl,-wrap,free  -Wl,-wrap,malloc -Wl,-wrap,realloc -Wl,-wrap,calloc")
    add_definitions(-DENABLE_MEM_DEBUG)
    message(STATUS "已启用内存调试功能")
endif ()

if (ENABLE_ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    message(STATUS "已启用Address Sanitize")
endif ()

set(LINK_LIB_LIST zlmediakit zltoolkit)

#默认链接jemalloc库避免内存碎片
find_package(JEMALLOC QUIET)
if (JEMALLOC_FOUND)
    message(STATUS "found library:\"${JEMALLOC_LIBRARIES}\"")
    include_directories(${JEMALLOC_INCLUDE_DIR})
    list(APPEND LINK_LIB_LIST ${JEMALLOC_LIBRARIES})
endif ()

#查找openssl是否安装
find_package(OpenSSL QUIET)
if (OPENSSL_FOUND AND ENABLE_OPENSSL)
    message(STATUS "found library:${OPENSSL_LIBRARIES},ENABLE_OPENSSL defined")
    include_directories(${OPENSSL_INCLUDE_DIR})
    add_definitions(-DENABLE_OPENSSL)
    list(APPEND LINK_LIB_LIST ${OPENSSL_LIBRARIES})
    if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND OPENSSL_USE_STATIC_LIBS)
        list(APPEND LINK_LIB_LIST dl)
    endif ()
else ()
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

#set(VS_FALGS "/wd4819 /wd4996 /wd4018 /wd4267 /wd4244 /wd4101 /wd4828 /wd4309 /wd4573 /wd4996" )
set(VS_FALGS "/wd4819")

#添加mpeg用于支持ts生成
if (ENABLE_HLS)
    message(STATUS "ENABLE_HLS defined")
    add_definitions(-DENABLE_HLS)

    include_directories(${MediaServer_Root}/libmpeg/include)
    aux_source_directory(${MediaServer_Root}/libmpeg/include src_mpeg)
    aux_source_directory(${MediaServer_Root}/libmpeg/source src_mpeg)

    add_library(mpeg STATIC ${src_mpeg})
    list(APPEND LINK_LIB_LIST mpeg)
    list(APPEND CXX_API_TARGETS mpeg)

    if (MSVC)
        set_target_properties(mpeg PROPERTIES COMPILE_FLAGS ${VS_FALGS})
    endif ()
endif ()

#添加mov、flv库用于MP4录制
if (ENABLE_MP4)
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

    if (MSVC)
        set_target_properties(mov flv PROPERTIES COMPILE_FLAGS ${VS_FALGS})
    endif ()
endif ()

#添加rtp库用于rtp转ps/ts
if (ENABLE_RTPPROXY AND ENABLE_HLS)
    message(STATUS "ENABLE_RTPPROXY defined")
    add_definitions(-DENABLE_RTPPROXY)
endif ()

#收集源代码
file(GLOB ToolKit_src_list ${ToolKit_Root}/*/*.cpp ${ToolKit_Root}/*/*.h ${ToolKit_Root}/*/*.c)
if (IOS)
    list(APPEND ToolKit_src_list ${ToolKit_Root}/Network/Socket_ios.mm)
endif ()

file(GLOB MediaKit_src_list ${MediaKit_Root}/*/*.cpp ${MediaKit_Root}/*/*.h ${MediaKit_Root}/*/*.c)

#去除win32的适配代码
if (NOT WIN32)
    list(REMOVE_ITEM ToolKit_src_list ${ToolKit_Root}/win32/getopt.c)
else ()
    #防止Windows.h包含Winsock.h
    add_definitions(-DWIN32_LEAN_AND_MEAN -DMP4V2_NO_STDINT_DEFS -D_CRT_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS)
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
    set(INSTALL_PATH_LIB lib${LIB_SUFFIX})
    set(INSTALL_PATH_INCLUDE include)
    set(INSTALL_PATH_EXECUTABLE bin)
endif ()

if (ENABLE_CXX_API)
    # 保留目录结构
    install(DIRECTORY ${ToolKit_Root}/ DESTINATION ${INSTALL_PATH_INCLUDE}/ZLToolKit REGEX "(.*[.](md|cpp)|win32)$" EXCLUDE)
    install(DIRECTORY ${MediaKit_Root}/ DESTINATION ${INSTALL_PATH_INCLUDE}/ZLMediaKit REGEX ".*[.](md|cpp)$" EXCLUDE)
    install(TARGETS ${CXX_API_TARGETS} DESTINATION ${INSTALL_PATH_LIB})
endif ()

if (WIN32)
    list(APPEND LINK_LIB_LIST WS2_32 Iphlpapi shlwapi)
    if (MSVC)
        set_target_properties(zltoolkit PROPERTIES COMPILE_FLAGS ${VS_FALGS})
        set_target_properties(zlmediakit PROPERTIES COMPILE_FLAGS ${VS_FALGS})
    endif ()
elseif (NOT ANDROID OR IOS)
    list(APPEND LINK_LIB_LIST pthread)
endif ()

#复制文件过来
execute_process(COMMAND cp -r ${CMAKE_CURRENT_SOURCE_DIR}/www ${EXECUTABLE_OUTPUT_PATH}/)
execute_process(COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/conf/config.ini ${EXECUTABLE_OUTPUT_PATH}/)

#添加c库
if (ENABLE_API)
    add_subdirectory(api)
endif ()

if (ENABLE_WEBRTC)
    #查找srtp是否安装
    find_package(SRTP QUIET)
    if (SRTP_FOUND)
        message(STATUS "found library:${SRTP_LIBRARIES}")
        include_directories(${SRTP_INCLUDE_DIRS})
        list(APPEND LINK_LIB_LIST ${SRTP_LIBRARIES})

        add_definitions(-DENABLE_WEBRTC)
        include_directories(./webrtc)
        file(GLOB SRC_WEBRTC_LIST ./webrtc/*.cpp ./webrtc/*.h ./webrtc/*.hpp)
        add_library(webrtc ${SRC_WEBRTC_LIST})
        list(APPEND LINK_LIB_LIST webrtc)
        message(STATUS "webrtc功能已开启")
    else ()
        message(WARNING "srtp未找到, webrtc相关功能打开失败")
    endif ()
endif ()

#ios不编译可执行程序
if (IOS)
    return()
endif()

#测试程序
if (ENABLE_TESTS)
    add_subdirectory(tests)
endif ()

#主服务器
if (ENABLE_SERVER)
    add_subdirectory(server)
endif ()

#播放器
if (ENABLE_PLAYER)
    add_subdirectory(player)
endif ()
