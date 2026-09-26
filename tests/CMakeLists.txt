# MIT License
#
# Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
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

find_package(PCAP QUIET)

# 门禁用例白名单：仅收录不依赖网络、命令行参数与外部文件，且执行完即以退出码
# 表明结果的测试程序。本目录其余程序多为服务型或压测工具(运行后不会自行退出)，
# 全量注册会使 ctest 永远挂起，故采用白名单而非自动收录。
# Gating test whitelist: only programs that need no network, no command-line arguments and no
# external files, and that finish on their own and report the outcome through the exit code.
# Most of the other programs here are long-running services or benchmarks that never exit by
# themselves, so registering everything would hang ctest; hence a whitelist rather than
# automatic discovery.
# 仅收录仓库中确实存在、且已实测可通过的用例。其余候选的现状：
#   test_vp9_rtp                        当前为失败状态，待单独修复后并入
#   test_rtcp_nack/test_webrtc_regression  仅在启用 WebRTC 时构建，尚未验证
# Only cases that exist in the repository and are verified to pass are listed. The remaining
# candidates: test_vp9_rtp currently fails and will be added once fixed; test_rtcp_nack and
# test_webrtc_regression are built only with WebRTC enabled and are not verified yet.
set(GATING_TESTS
  test_av1_rtmp
  test_amf
  test_sortor)

foreach(TEST_SRC ${TEST_SRC_LIST})
  get_filename_component(TEST_EXE_NAME ${TEST_SRC} NAME_WE)

  if(NOT PCAP_FOUND)
    # message(WARNING "PCAP 未找到")
    if("${TEST_EXE_NAME}" MATCHES "test_rtp_pcap")
      continue()
    endif()
  endif()

  if(NOT TARGET ZLMediaKit::WebRTC)
    # 暂时过滤掉依赖 WebRTC 的测试模块
    if("${TEST_EXE_NAME}" MATCHES "test_rtcp_nack|test_webrtc_regression|test_turn_allocation")
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

  if(${TEST_EXE_NAME} IN_LIST GATING_TESTS)
    add_test(NAME ${TEST_EXE_NAME} COMMAND ${TEST_EXE_NAME})
    # 单个用例正常在秒级内结束，超时即判失败，避免卡住整条流水线
    # A single case finishes within seconds; time out instead of stalling the pipeline
    set_tests_properties(${TEST_EXE_NAME} PROPERTIES TIMEOUT 120)
  endif()
endforeach()

if(TARGET test_rtp_pcap)
  target_include_directories(test_rtp_pcap SYSTEM PRIVATE ${PCAP_INCLUDE_DIRS})
  target_link_libraries(test_rtp_pcap  ${PCAP_LIBRARIES})
endif()
