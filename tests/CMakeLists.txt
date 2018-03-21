#查找SDL是否安装
find_package(SDL QUIET)
if(SDL_FOUND)
    include_directories(${SDL_INCLUDE_DIR})
    list(APPEND  LINK_LIB_LIST ${SDL_LIBRARY})
    message(STATUS "找到SDL")
endif()

#查找ffmpeg/libutil是否安装
find_package(AVUTIL QUIET)
if(AVUTIL_FOUND)
    include_directories(${AVUTIL_INCLUDE_DIR})
    list(APPEND  LINK_LIB_LIST ${AVUTIL_LIBRARIES})
    message(STATUS "找到libutil")
endif()

#查找ffmpeg/libavcodec是否安装
find_package(AVCODEC QUIET)
if(AVCODEC_FOUND)
    include_directories(${AVCODEC_INCLUDE_DIR})
    list(APPEND  LINK_LIB_LIST ${AVCODEC_LIBRARIES})
    message(STATUS "找到libavcodec")
endif()

aux_source_directory(. TEST_SRC_LIST)
#如果ffmpeg/libavcodec ffmpeg/libavcodec SDL 都安装了则编译 test_player
if(SDL_FOUND AND AVCODEC_FOUND AND AVUTIL_FOUND)
    message(STATUS "test_player被编译")
else()
    message(STATUS "test_player被忽略,如需编译，请先安装sdl ffmpeg/libavcodec ffmpeg/libavcodec")
    list(REMOVE_ITEM TEST_SRC_LIST ./test_player.cpp)
endif()

foreach(TEST_SRC ${TEST_SRC_LIST})
    STRING(REGEX REPLACE "^\\./|\\.c[a-zA-Z0-9_]*$" "" TEST_EXE_NAME ${TEST_SRC})
    message(STATUS "添加测试程序:${TEST_EXE_NAME}")
    add_executable(${TEST_EXE_NAME} ${TEST_SRC})

    if(ANDROID)
        target_link_libraries(${TEST_EXE_NAME} ${CMAKE_PROJECT_NAME}_static ${LINK_LIB_LIST})
    elseif(WIN32)
        target_link_libraries(${TEST_EXE_NAME} ${CMAKE_PROJECT_NAME}_shared ${LINK_LIB_LIST} )
    else()
        target_link_libraries(${TEST_EXE_NAME} ${CMAKE_PROJECT_NAME}_shared ${LINK_LIB_LIST} pthread)
    endif()

endforeach()














