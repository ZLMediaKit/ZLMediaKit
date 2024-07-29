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

include(GenerateExportHeader)

file(GLOB MediaKit_SRC_LIST
  ${CMAKE_CURRENT_SOURCE_DIR}/*/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/*/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/*/*.h)

if(USE_SOLUTION_FOLDERS AND (NOT GROUP_BY_EXPLORER))
  # 在 IDE 中对文件进行分组, 源文件和头文件分开
  set_file_group("${CMAKE_CURRENT_SOURCE_DIR}" ${MediaKit_SRC_LIST})
endif()

if(NOT ENABLE_CXX_API OR IOS)
  # 添加库
  add_library(zlmediakit STATIC ${MediaKit_SRC_LIST})
  add_library(ZLMediaKit::MediaKit ALIAS zlmediakit)
  list(APPEND COMPILE_DEFINITIONS MediaKit_STATIC)
else()
  # 在 Windows 下启用导出所有符号的选项
  if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
  endif()
  # 添加动态库
  add_definitions(-DMediaKit_EXPORTS)
  add_library(zlmediakit SHARED ${MediaKit_SRC_LIST})
  add_library(ZLMediaKit::MediaKit ALIAS zlmediakit)
  target_link_libraries(zlmediakit
          PUBLIC ZLMediaKit::ext-codec)
  # 设置生成的导出库文件 .lib（对于 Windows）
  set_target_properties(zlmediakit PROPERTIES
          OUTPUT_NAME zlmediakit  # 输出的 DLL 文件名
  )
endif ()

set(LINK_LIBRARIES ${MK_LINK_LIBRARIES})
list(REMOVE_ITEM LINK_LIBRARIES ZLMediaKit::MediaKit)
set(COMPILE_DEFINITIONS ${MK_COMPILE_DEFINITIONS})

list(APPEND COMPILE_DEFINITIONS GENERATE_EXPORT)

target_compile_definitions(zlmediakit
  PUBLIC ${COMPILE_DEFINITIONS})
target_compile_options(zlmediakit
  PRIVATE ${COMPILE_OPTIONS_DEFAULT})
target_link_libraries(zlmediakit
  PUBLIC ${LINK_LIBRARIES})
target_include_directories(zlmediakit
        PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"

        PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
)

update_cached_list(MK_LINK_LIBRARIES ZLMediaKit::MediaKit)


generate_export_header(zlmediakit
        EXPORT_MACRO_NAME MK_EXPORT
        BASE_NAME MK_CXX
        STATIC_DEFINE MediaKit_STATIC
        EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/mk_cxx_export.h")

# 未在使用
if(ENABLE_CXX_API)
  # 保留目录结构
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION ${INSTALL_PATH_INCLUDE}/ZLMediaKit
    REGEX ".*[.](md|cpp)$" EXCLUDE)
    if(ENABLE_CXX_API)
      install(TARGETS zlmediakit
        DESTINATION ${INSTALL_PATH_BIN})
    else()
      install(TARGETS zlmediakit
        DESTINATION ${INSTALL_PATH_LIB})
    endif()
endif ()
