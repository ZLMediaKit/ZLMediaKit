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

set(LINK_LIBRARIES ${MK_LINK_LIBRARIES})

find_package(PkgConfig QUIET)
# 查找 SDL2 是否安装
if(PKG_CONFIG_FOUND)
  pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
  if(SDL2_FOUND)
    list(APPEND LINK_LIBRARIES PkgConfig::SDL2)
    message(STATUS "found library: ${SDL2_LIBRARIES}")
  endif()
else()
  find_package(SDL2 QUIET)
  if(SDL2_FOUND)
    include_directories(SYSTEM ${SDL2_INCLUDE_DIR})
    list(APPEND LINK_LIBRARIES ${SDL2_LIBRARY})
    message(STATUS "found library: ${SDL2_LIBRARY}")
  endif()
endif()

set(PLAYER_NAME "test_player")

# 如果 ffmpeg/libavcodec ffmpeg/libavcodec SDL 都安装了则编译播放器
if(NOT SDL2_FOUND)
  message(WARNING "${PLAYER_NAME} disabled, please install sdl2 ffmpeg/libavcodec ffmpeg/libavutil ffmpeg/libswresample")
  return()
endif()

message(STATUS "${PLAYER_NAME} enabled")

aux_source_directory(. SRC_LIST)
add_executable(${PLAYER_NAME} ${SRC_LIST})
target_compile_definitions(${PLAYER_NAME}
  PRIVATE ${MK_COMPILE_DEFINITIONS})
target_compile_options(${PLAYER_NAME}
  PRIVATE ${COMPILE_OPTIONS_DEFAULT})

# TODO: 统一参数?
if(MSVC)
  set_target_properties(${PLAYER_NAME} PROPERTIES LINK_FLAGS "/SAFESEH:NO /SUBSYSTEM:WINDOWS")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  target_link_libraries(${PLAYER_NAME} -Wl,--start-group ${LINK_LIBRARIES} -Wl,--end-group)
else()
  target_link_libraries(${PLAYER_NAME} ${LINK_LIBRARIES})
endif()
