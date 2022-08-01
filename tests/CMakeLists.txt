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

aux_source_directory(. TEST_SRC_LIST)

foreach(TEST_SRC ${TEST_SRC_LIST})
  get_filename_component(TEST_EXE_NAME ${TEST_SRC} NAME_WE)

  if(NOT TARGET ZLMediaKit::WebRTC)
    # 暂时过滤掉依赖 WebRTC 的测试模块
    if("${TEST_EXE_NAME}" MATCHES "test_rtcp_nack")
      continue()
    endif()
  endif()

  message(STATUS "add test: ${TEST_EXE_NAME}")
  add_executable(${TEST_EXE_NAME} ${TEST_SRC})
  target_compile_options(${TEST_EXE_NAME}
    PRIVATE ${COMPILE_OPTIONS_DEFAULT})

  target_compile_definitions(${TEST_EXE_NAME}
    PRIVATE ${MK_COMPILE_DEFINITIONS})

  if(USE_SOLUTION_FOLDERS)
    SET_PROPERTY(TARGET ${TEST_EXE_NAME} PROPERTY FOLDER "test")
  endif()

  if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_libraries(${TEST_EXE_NAME} -Wl,--start-group ${MK_LINK_LIBRARIES} -Wl,--end-group)
  else()
    target_link_libraries(${TEST_EXE_NAME} ${MK_LINK_LIBRARIES})
  endif()
endforeach()
