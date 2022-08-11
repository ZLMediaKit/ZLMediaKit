# MIT License
#
# Copyright (c) 2016-2022 The ZLMediaKit project authors. All Rights Reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

##############################################################################

# jsoncpp
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/jsoncpp JSONCPP_SRC_LIST)
add_library(jsoncpp STATIC ${JSONCPP_SRC_LIST})
target_compile_options(jsoncpp
  PRIVATE ${COMPILE_OPTIONS_DEFAULT})
target_include_directories(jsoncpp
  PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

update_cached_list(MK_LINK_LIBRARIES jsoncpp)

##############################################################################

# media-server
set(MediaServer_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/media-server")
# TODO: 补一个函数处理各种库

# 添加 mov、flv 库用于 MP4 录制
if(ENABLE_MP4)
  message(STATUS "ENABLE_MP4 defined")

  # MOV
  set(MediaServer_MOV_ROOT ${MediaServer_ROOT}/libmov)
  aux_source_directory(${MediaServer_MOV_ROOT}/include MOV_SRC_LIST)
  aux_source_directory(${MediaServer_MOV_ROOT}/source  MOV_SRC_LIST)
  add_library(mov STATIC ${MOV_SRC_LIST})
  add_library(MediaServer::mov ALIAS mov)
  target_compile_definitions(mov
    PUBLIC -DENABLE_MP4)
  target_compile_options(mov
    PRIVATE ${COMPILE_OPTIONS_DEFAULT})
  target_include_directories(mov
    PRIVATE
      "$<BUILD_INTERFACE:${MediaServer_MOV_ROOT}/include>"
    PUBLIC
      "$<BUILD_INTERFACE:${MediaServer_MOV_ROOT}/include>")

  # FLV
  set(MediaServer_FLV_ROOT ${MediaServer_ROOT}/libflv)
  aux_source_directory(${MediaServer_FLV_ROOT}/include FLV_SRC_LIST)
  aux_source_directory(${MediaServer_FLV_ROOT}/source  FLV_SRC_LIST)
  add_library(flv STATIC ${FLV_SRC_LIST})
  add_library(MediaServer::flv ALIAS flv)
  target_compile_options(flv
    PRIVATE ${COMPILE_OPTIONS_DEFAULT})
  target_include_directories(flv
    PRIVATE
      "$<BUILD_INTERFACE:${MediaServer_FLV_ROOT}/include>"
    PUBLIC
      "$<BUILD_INTERFACE:${MediaServer_FLV_ROOT}/include>")

  update_cached_list(MK_LINK_LIBRARIES
    MediaServer::flv MediaServer::mov)
  update_cached_list(MK_COMPILE_DEFINITIONS
    ENABLE_MP4)
endif()

# 添加 mpeg 用于支持 ts 生成
if(ENABLE_RTPPROXY OR ENABLE_HLS)
  # mpeg
  set(MediaServer_MPEG_ROOT ${MediaServer_ROOT}/libmpeg)
  aux_source_directory(${MediaServer_MPEG_ROOT}/include MPEG_SRC_LIST)
  aux_source_directory(${MediaServer_MPEG_ROOT}/source  MPEG_SRC_LIST)
  add_library(mpeg STATIC ${MPEG_SRC_LIST})
  add_library(MediaServer::mpeg ALIAS mpeg)
  target_compile_options(mpeg
    PRIVATE ${COMPILE_OPTIONS_DEFAULT})
  target_include_directories(mpeg
    PRIVATE
      "$<BUILD_INTERFACE:${MediaServer_MPEG_ROOT}/include>"
    PUBLIC
      "$<BUILD_INTERFACE:${MediaServer_MPEG_ROOT}/include>")

  update_cached_list(MK_LINK_LIBRARIES MediaServer::mpeg)
  if(ENABLE_RTPPROXY)
    update_cached_list(MK_COMPILE_DEFINITIONS ENABLE_RTPPROXY)
  endif()
  if(ENABLE_HLS)
    update_cached_list(MK_COMPILE_DEFINITIONS ENABLE_HLS)
  endif()
endif()

##############################################################################

# toolkit
# TODO: 改造 toolkit 以便直接引用

include(CheckStructHasMember)
include(CheckSymbolExists)

# 检查 sendmmsg 相关依赖并设置对应的宏, 配置 _GNU_SOURCE 以启用 GNU 扩展特性
list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
check_struct_has_member("struct mmsghdr" msg_hdr sys/socket.h HAVE_MMSG_HDR)
check_symbol_exists(sendmmsg sys/socket.h HAVE_SENDMMSG_API)
check_symbol_exists(recvmmsg sys/socket.h HAVE_RECVMMSG_API)

set(COMPILE_DEFINITIONS)
# ToolKit 依赖 ENABLE_OPENSSL 以及 ENABLE_MYSQL
list(FIND MK_COMPILE_DEFINITIONS ENABLE_OPENSSL ENABLE_OPENSSL_INDEX)
if(NOT ENABLE_OPENSSL_INDEX EQUAL -1)
  list(APPEND COMPILE_DEFINITIONS ENABLE_OPENSSL)
endif()
list(FIND MK_COMPILE_DEFINITIONS ENABLE_MYSQL   ENABLE_MYSQL_INDEX)
if(NOT ENABLE_MYSQL_INDEX EQUAL -1)
  list(APPEND COMPILE_DEFINITIONS ENABLE_MYSQL)
endif()
if(HAVE_MMSG_HDR)
  list(APPEND COMPILE_DEFINITIONS HAVE_MMSG_HDR)
endif()
if(HAVE_SENDMMSG_API)
  list(APPEND COMPILE_DEFINITIONS HAVE_SENDMMSG_API)
endif()
if(HAVE_RECVMMSG_API)
  list(APPEND COMPILE_DEFINITIONS HAVE_RECVMMSG_API)
endif()

set(ToolKit_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/ZLToolKit)
# 收集源代码
file(GLOB ToolKit_SRC_LIST
  ${ToolKit_ROOT}/src/*/*.cpp
  ${ToolKit_ROOT}/src/*/*.h
  ${ToolKit_ROOT}/src/*/*.c)
if(IOS)
  list(APPEND ToolKit_SRC_LIST
    ${ToolKit_ROOT}/Network/Socket_ios.mm)
endif()

###################################################################
#使用wepoll windows iocp 模拟 epoll
if(ENABLE_WEPOLL)
  if(WIN32)
    message(STATUS "Enable wepoll")
    #增加wepoll源文件及api参数兼容文件
    list(APPEND ToolKit_SRC_LIST
            ${CMAKE_CURRENT_SOURCE_DIR}/wepoll/wepoll.c
            ${CMAKE_CURRENT_SOURCE_DIR}/wepoll/sys/epoll.cpp)
    #增加wepoll头文件目录
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/wepoll)
    #开启epoll
    add_definitions(-DHAS_EPOLL)
  endif()
endif()
###################################################################

# 去除 win32 的适配代码
if(NOT WIN32)
  list(REMOVE_ITEM ToolKit_SRC_LIST ${ToolKit_ROOT}/win32/getopt.c)
else()
  # 防止 Windows.h 包含 Winsock.h
  list(APPEND COMPILE_DEFINITIONS
    WIN32_LEAN_AND_MEAN MP4V2_NO_STDINT_DEFS
    # 禁用警告
    _CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)
endif()

# 添加库
add_library(zltoolkit STATIC ${ToolKit_SRC_LIST})
add_library(ZLMediaKit::ToolKit ALIAS zltoolkit)
target_compile_definitions(zltoolkit
  PUBLIC ${COMPILE_DEFINITIONS})
target_compile_options(zltoolkit
  PRIVATE ${COMPILE_OPTIONS_DEFAULT})
target_include_directories(zltoolkit
  PRIVATE
    "$<BUILD_INTERFACE:${ToolKit_ROOT}/src>"
  PUBLIC
    "$<BUILD_INTERFACE:${ToolKit_ROOT}>/src")

update_cached_list(MK_LINK_LIBRARIES ZLMediaKit::ToolKit)

if(USE_SOLUTION_FOLDERS AND (NOT GROUP_BY_EXPLORER))
  # 在 IDE 中对文件进行分组, 源文件和头文件分开
  set_file_group(${ToolKit_ROOT}/src ${ToolKit_SRC_LIST})
endif()

# 未在使用
if(ENABLE_CXX_API)
  # 保留目录结构
  install(DIRECTORY ${ToolKit_ROOT}/
    DESTINATION ${INSTALL_PATH_INCLUDE}/ZLToolKit
    REGEX "(.*[.](md|cpp)|win32)$" EXCLUDE)
  install(TARGETS zltoolkit
    DESTINATION ${INSTALL_PATH_LIB})
endif()
