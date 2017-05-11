#查找SDL是否安装
find_package(SDL QUIET)
if(SDL_FOUND)
include_directories(${SDL_INCLUDE_DIR})
list(APPEND  LINK_LIB_LIST ${SDL_LIBRARY})
message(STATUS "找到SDL")
endif(SDL_FOUND)

#查找ffmpeg/libutil是否安装
find_package(AVUTIL QUIET)
if(AVUTIL_FOUND)
include_directories(${AVUTIL_INCLUDE_DIR})
list(APPEND  LINK_LIB_LIST ${AVUTIL_LIBRARIES})
message(STATUS "找到libutil")
endif(AVUTIL_FOUND)

#查找ffmpeg/libavcodec是否安装
find_package(AVCODEC QUIET)
if(AVCODEC_FOUND)
include_directories(${AVCODEC_INCLUDE_DIR})
list(APPEND  LINK_LIB_LIST ${AVCODEC_LIBRARIES})
message(STATUS "找到libavcodec")
endif(AVCODEC_FOUND)

aux_source_directory(. TEST_SRC_LIST)
#如果ffmpeg/libavcodec ffmpeg/libavcodec SDL 都安装了则编译 test_player
if(SDL_FOUND AND AVCODEC_FOUND AND AVUTIL_FOUND)
message(STATUS "test_player被编译")
else(SDL_FOUND AND AVCODEC_FOUND AND AVUTIL_FOUND)
message(STATUS "test_player被忽略,如需编译，请先安装sdl ffmpeg/libavcodec ffmpeg/libavcodec")
list(REMOVE_ITEM TEST_SRC_LIST ./test_player.cpp)
endif(SDL_FOUND AND AVCODEC_FOUND AND AVUTIL_FOUND)

foreach(TEST_SRC ${TEST_SRC_LIST})
STRING(REGEX REPLACE ".cpp" "" TEST_EXE_NAME ${TEST_SRC})
STRING(REGEX REPLACE "./" "" TEST_EXE_NAME ${TEST_EXE_NAME})
message(STATUS "add test program:${TEST_EXE_NAME}")
add_executable(${TEST_EXE_NAME} ${TEST_SRC})
target_link_libraries(${TEST_EXE_NAME} ${CMAKE_PROJECT_NAME}_shared ${LINK_LIB_LIST} pthread) 
endforeach(TEST_SRC ${TEST_SRC_LIST})














