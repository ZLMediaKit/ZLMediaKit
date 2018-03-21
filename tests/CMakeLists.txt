#查找SDL是否安装
find_package(SDL QUIET)
if(SDL_FOUND)
    include_directories(${SDL_INCLUDE_DIR})
    list(APPEND  LINK_LIB_LIST ${SDL_LIBRARY})
    message(STATUS " found  SDL")
endif()

#查找ffmpeg/libutil是否安装
find_package(AVUTIL QUIET)
if(AVUTIL_FOUND)
    include_directories(${AVUTIL_INCLUDE_DIR})
    list(APPEND  LINK_LIB_LIST ${AVUTIL_LIBRARIES})
    message(STATUS " found  libutil")
endif()

#查找ffmpeg/libavcodec是否安装
find_package(AVCODEC QUIET)
if(AVCODEC_FOUND)
    include_directories(${AVCODEC_INCLUDE_DIR})
    list(APPEND  LINK_LIB_LIST ${AVCODEC_LIBRARIES})
    message(STATUS " found  libavcodec")
endif()

aux_source_directory(. TEST_SRC_LIST)
#如果ffmpeg/libavcodec ffmpeg/libavcodec SDL 都安装了则编译 test_player
if(SDL_FOUND AND AVCODEC_FOUND AND AVUTIL_FOUND)
    message(STATUS "test_player will be compiled")
else()
    message(STATUS "test_player ingored, please install sdl ffmpeg/libavcodec ffmpeg/libavcodec")
    list(REMOVE_ITEM TEST_SRC_LIST ./test_player.cpp)
endif()

foreach(TEST_SRC ${TEST_SRC_LIST})
    STRING(REGEX REPLACE "^\\./|\\.c[a-zA-Z0-9_]*$" "" TEST_EXE_NAME ${TEST_SRC})
    message(STATUS "add test:${TEST_EXE_NAME}")
    add_executable(${TEST_EXE_NAME} ${TEST_SRC})
    target_link_libraries(${TEST_EXE_NAME} ${LINK_LIB_LIST})
endforeach()














