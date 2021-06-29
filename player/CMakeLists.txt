find_package(PkgConfig QUIET)

#查找SDL2是否安装
if (PKG_CONFIG_FOUND)
    pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
    if (SDL2_FOUND)
        include_directories(${SDL2_INCLUDE_DIRS})
        link_directories(${SDL2_LIBRARY_DIRS})
        list(APPEND LINK_LIB_LIST ${SDL2_LIBRARIES})
        message(STATUS "found library:${SDL2_LIBRARIES}")
    endif ()
else ()
    find_package(SDL2 QUIET)
    if (SDL2_FOUND)
        include_directories(${SDL2_INCLUDE_DIR})
        list(APPEND LINK_LIB_LIST ${SDL2_LIBRARY})
        message(STATUS "found library:${SDL2_LIBRARY}")
    endif (SDL2_FOUND)
endif ()

#查找ffmpeg/libutil是否安装
if (PKG_CONFIG_FOUND)
    pkg_check_modules(AVUTIL QUIET IMPORTED_TARGET libavutil)
    if (AVUTIL_FOUND)
        include_directories(${AVUTIL_INCLUDE_DIRS})
        link_directories(${AVUTIL_LIBRARY_DIRS})
        list(APPEND LINK_LIB_LIST ${AVUTIL_LIBRARIES})
        message(STATUS "found library:${AVUTIL_LIBRARIES}")
    endif ()
else ()
    find_package(AVUTIL QUIET)
    if (AVUTIL_FOUND)
        include_directories(${AVUTIL_INCLUDE_DIR})
        list(APPEND LINK_LIB_LIST ${AVUTIL_LIBRARIES})
        message(STATUS "found library:${AVUTIL_LIBRARIES}")
    endif ()
endif ()

#查找ffmpeg/libswresample是否安装
if (PKG_CONFIG_FOUND)
    pkg_check_modules(SWRESAMPLE QUIET IMPORTED_TARGET libswresample)
    if (SWRESAMPLE_FOUND)
        include_directories(${SWRESAMPLE_INCLUDE_DIRS})
        link_directories(${SWRESAMPLE_LIBRARY_DIRS})
        list(APPEND LINK_LIB_LIST ${SWRESAMPLE_LIBRARIES})
        message(STATUS "found library:${SWRESAMPLE_LIBRARIES}")
    endif ()
else ()
    find_package(SWRESAMPLE QUIET)
    if (SWRESAMPLE_FOUND)
        include_directories(${SWRESAMPLE_INCLUDE_DIR})
        list(APPEND LINK_LIB_LIST ${SWRESAMPLE_LIBRARIES})
        message(STATUS "found library:${SWRESAMPLE_LIBRARIES}")
    endif ()
endif ()

#查找ffmpeg/libavcodec是否安装
if (PKG_CONFIG_FOUND)
    pkg_check_modules(AVCODEC QUIET IMPORTED_TARGET libavcodec)
    if (AVCODEC_FOUND)
        include_directories(${AVCODEC_INCLUDE_DIRS})
        link_directories(${AVCODEC_LIBRARY_DIRS})
        list(APPEND LINK_LIB_LIST ${AVCODEC_LIBRARIES})
        message(STATUS "found library:${AVCODEC_LIBRARIES}")
    endif ()
else ()
    find_package(AVCODEC QUIET)
    if (AVCODEC_FOUND)
        include_directories(${AVCODEC_INCLUDE_DIR})
        list(APPEND LINK_LIB_LIST ${AVCODEC_LIBRARIES})
        message(STATUS "found library:${AVCODEC_LIBRARIES}")
    endif ()
endif ()

set(PLAYER_NAME "test_player")

#如果ffmpeg/libavcodec ffmpeg/libavcodec SDL 都安装了则编译播放器
if (SDL2_FOUND AND AVCODEC_FOUND AND AVUTIL_FOUND AND SWRESAMPLE_FOUND)
    message(STATUS "${PLAYER_NAME} enabled")
else ()
    message(STATUS "${PLAYER_NAME} disabled, please install sdl2 ffmpeg/libavcodec ffmpeg/libavutil ffmpeg/libswresample")
    return()
endif ()

aux_source_directory(. SRC_LIST)
add_executable(${PLAYER_NAME} ${SRC_LIST})

if (MSVC)
    set_target_properties(${PLAYER_NAME} PROPERTIES COMPILE_FLAGS ${VS_FALGS})
    set_target_properties(${PLAYER_NAME} PROPERTIES LINK_FLAGS "/SAFESEH:NO /SUBSYSTEM:WINDOWS")
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_libraries(${PLAYER_NAME} -Wl,--start-group ${LINK_LIB_LIST} -Wl,--end-group)
else ()
    target_link_libraries(${PLAYER_NAME} ${LINK_LIB_LIST})
endif ()














