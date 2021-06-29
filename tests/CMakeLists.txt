execute_process(COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/ssl.p12 ${EXECUTABLE_OUTPUT_PATH}/)
aux_source_directory(. TEST_SRC_LIST)

foreach (TEST_SRC ${TEST_SRC_LIST})
    if (NOT ENABLE_WEBRTC)
        # 暂时过滤掉依赖 WebRTC 的测试模块
        if ("${TEST_SRC}" MATCHES "test_rtcp_nack\.cpp")
            continue()
        endif ()
    endif ()
    STRING(REGEX REPLACE "^\\./|\\.c[a-zA-Z0-9_]*$" "" TEST_EXE_NAME ${TEST_SRC})
    message(STATUS "add test:${TEST_EXE_NAME}")
    add_executable(${TEST_EXE_NAME} ${TEST_SRC})

    if (MSVC)
        set_target_properties(${TEST_EXE_NAME} PROPERTIES COMPILE_FLAGS ${VS_FALGS})
    endif ()

    if (CMAKE_SYSTEM_NAME MATCHES "Linux")
        target_link_libraries(${TEST_EXE_NAME} -Wl,--start-group ${LINK_LIB_LIST} -Wl,--end-group)
    else ()
        target_link_libraries(${TEST_EXE_NAME} ${LINK_LIB_LIST})
    endif ()
endforeach ()












