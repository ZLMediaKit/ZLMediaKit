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

# 查找 srtp 是否安装
find_package(SRTP QUIET)
if(NOT SRTP_FOUND OR NOT ENABLE_OPENSSL)
  set(ENABLE_WEBRTC OFF)
  message(WARNING "srtp 未找到, WebRTC 相关功能打开失败")
  return()
endif()


message(STATUS "found library: ${SRTP_LIBRARIES}")
include_directories(SYSTEM ${SRTP_INCLUDE_DIRS})

set(LINK_LIBRARIES ${SRTP_LIBRARIES})

set(COMPILE_DEFINITIONS)
set(INCLUDE_DIRECTORIES)

find_package(SCTP QUIET)
if(SCTP_FOUND)
  message(STATUS "found library: ${SCTP_INCLUDE_DIRS} ${SCTP_LIBRARIES}")
  include_directories(SYSTEM ${SCTP_INCLUDE_DIRS})
  # TODO: 避免暴露 SCTP 到 WebRTC 模块外
  list(APPEND COMPILE_DEFINITIONS ENABLE_SCTP)
  list(APPEND INCLUDE_DIRECTORIES ${SCTP_INCLUDE_DIRS})
  list(APPEND LINK_LIBRARIES ${SCTP_LIBRARIES})
  message(STATUS "WebRTC datachannel 功能已打开")
endif()

file(GLOB WEBRTC_SRC_LIST
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)
add_library(webrtc ${WEBRTC_SRC_LIST})
add_library(ZLMediaKit::WebRTC ALIAS webrtc)
target_compile_definitions(webrtc
  # ENABLE_SCTP 暂时需要暴露
  PUBLIC ${COMPILE_DEFINITIONS})
target_compile_options(webrtc
  PRIVATE ${COMPILE_OPTIONS_DEFAULT})
target_link_libraries(webrtc
  PRIVATE
    ZLMediaKit::MediaKit
    ZLMediaKit::ToolKit
    ${LINK_LIBRARIES})
target_include_directories(webrtc
  PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    ${INCLUDE_DIRECTORIES})

message(STATUS "WebRTC 功能已开启")

update_cached_list(MK_COMPILE_DEFINITIONS ENABLE_WEBRTC)
update_cached_list(MK_LINK_LIBRARIES ZLMediaKit::WebRTC ${LINK_LIBRARIES})
