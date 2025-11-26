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
file(GLOB JSONCPP_SRC_LIST
  ${CMAKE_CURRENT_SOURCE_DIR}/jsoncpp/include/json/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/jsoncpp/src/lib_json/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/jsoncpp/src/lib_json/*.h)
  
add_library(jsoncpp STATIC ${JSONCPP_SRC_LIST})
target_compile_options(jsoncpp
  PRIVATE ${COMPILE_OPTIONS_DEFAULT})
target_include_directories(jsoncpp
  PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/jsoncpp/include"
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/jsoncpp/include")

update_cached_list(MK_LINK_LIBRARIES jsoncpp)

##############################################################################

# media-server
set(MediaServer_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/media-server")
# TODO: 补一个函数处理各种库

# 添加 mov、flv 库用于 MP4 录制
if (ENABLE_MP4)
  # MOV
  set(MediaServer_MOV_ROOT ${MediaServer_ROOT}/libmov)
  aux_source_directory(${MediaServer_MOV_ROOT}/include MOV_SRC_LIST)
  aux_source_directory(${MediaServer_MOV_ROOT}/source MOV_SRC_LIST)
  add_library(mov STATIC ${MOV_SRC_LIST})
  add_library(MediaServer::mov ALIAS mov)
  target_compile_options(mov PRIVATE ${COMPILE_OPTIONS_DEFAULT})
  target_include_directories(mov
          PRIVATE
          "$<BUILD_INTERFACE:${MediaServer_MOV_ROOT}/include>"
          PUBLIC
          "$<BUILD_INTERFACE:${MediaServer_MOV_ROOT}/include>")

  # FLV
  set(MediaServer_FLV_ROOT ${MediaServer_ROOT}/libflv)
  aux_source_directory(${MediaServer_FLV_ROOT}/include FLV_SRC_LIST)
  aux_source_directory(${MediaServer_FLV_ROOT}/source FLV_SRC_LIST)
  add_library(flv STATIC ${FLV_SRC_LIST})
  add_library(MediaServer::flv ALIAS flv)
  target_compile_options(flv PRIVATE ${COMPILE_OPTIONS_DEFAULT})
  target_include_directories(flv
          PRIVATE
          "$<BUILD_INTERFACE:${MediaServer_FLV_ROOT}/include>"
          PUBLIC
          "$<BUILD_INTERFACE:${MediaServer_FLV_ROOT}/include>")

  update_cached_list(MK_LINK_LIBRARIES MediaServer::flv MediaServer::mov)

  if (ENABLE_MP4)
    message(STATUS "ENABLE_MP4 defined")
    update_cached_list(MK_COMPILE_DEFINITIONS ENABLE_MP4)
  endif ()
endif ()

# 添加 mpeg 用于支持 ts 生成
if(ENABLE_RTPPROXY OR ENABLE_HLS)
  # mpeg
  set(MediaServer_MPEG_ROOT ${MediaServer_ROOT}/libmpeg)
  aux_source_directory(${MediaServer_MPEG_ROOT}/include MPEG_SRC_LIST)
  aux_source_directory(${MediaServer_MPEG_ROOT}/source  MPEG_SRC_LIST)
  add_library(mpeg STATIC ${MPEG_SRC_LIST})
  add_library(MediaServer::mpeg ALIAS mpeg)
  # media-server库相关编译宏
  # MPEG_H26X_VERIFY - 视频流类型识别
  # MPEG_ZERO_PAYLOAD_LENGTH - 兼容hik流
  # MPEG_DAHUA_AAC_FROM_G711 - 兼容dahua流
  target_compile_options(mpeg
    PRIVATE ${COMPILE_OPTIONS_DEFAULT} -DMPEG_H26X_VERIFY -DMPEG_ZERO_PAYLOAD_LENGTH -DMPEG_DAHUA_AAC_FROM_G711)
  target_include_directories(mpeg
    PRIVATE
      "$<BUILD_INTERFACE:${MediaServer_MPEG_ROOT}/include>"
    PUBLIC
      "$<BUILD_INTERFACE:${MediaServer_MPEG_ROOT}/include>")

  update_cached_list(MK_LINK_LIBRARIES MediaServer::mpeg)
  if(ENABLE_RTPPROXY)
    message(STATUS "ENABLE_RTPPROXY defined")
    update_cached_list(MK_COMPILE_DEFINITIONS ENABLE_RTPPROXY)
  endif()
  if(ENABLE_HLS)
    message(STATUS "ENABLE_HLS defined")
    update_cached_list(MK_COMPILE_DEFINITIONS ENABLE_HLS)
  endif()
endif()

##############################################################################

# toolkit
add_subdirectory(ZLToolKit)
# 添加库别名
add_library(ZLMediaKit::ToolKit ALIAS ZLToolKit)
# 添加依赖
update_cached_list(MK_LINK_LIBRARIES ZLMediaKit::ToolKit)