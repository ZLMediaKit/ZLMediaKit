# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

#设置生成的so动态库最后输出的路径
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libs_export/${ANDROID_ABI})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libs_export/${ANDROID_ABI})

#设置工程源码根目录
set(ZLMediaKit_Root ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../)
set(JNI_Root ${CMAKE_CURRENT_SOURCE_DIR})
set(ToolKit_Root ${ZLMediaKit_Root}/3rdpart/ZLToolKit/src)
set(MediaKit_Root ${ZLMediaKit_Root}/src)

#设置头文件目录
INCLUDE_DIRECTORIES(${ToolKit_Root})
INCLUDE_DIRECTORIES(${MediaKit_Root})
INCLUDE_DIRECTORIES(${JNI_Root})

#收集源代码
file(GLOB ToolKit_src_list ${ToolKit_Root}/*/*.cpp ${ToolKit_Root}/*/*.h ${ToolKit_Root}/*/*.c)
file(GLOB MediaKit_src_list ${MediaKit_Root}/*/*.cpp ${MediaKit_Root}/*/*.h ${MediaKit_Root}/*/*.c)
file(GLOB JNI_src_list ${JNI_Root}/*.cpp ${JNI_Root}/*.h)

#去除win32的适配代码
list(REMOVE_ITEM ToolKit_src_list ${ToolKit_Root}/win32/getopt.c)

set(ENABLE_HLS true)
#默认禁用的库，如果要开启，请指定其路径
set(ENABLE_OPENSSL false)
set(ENABLE_MYSQL false)
set(ENABLE_MP4V2 false)
set(ENABLE_FAAC false)
set(ENABLE_X264 false)

#添加两个静态库
if(ENABLE_HLS)
    message(STATUS "ENABLE_HLS defined")
    add_definitions(-DENABLE_HLS)
    set(MediaServer_Root ${ZLMediaKit_Root}/3rdpart/media-server)
    set(LINK_LIB_LIST zlmediakit zltoolkit mpeg)
else()
    set(LINK_LIB_LIST zlmediakit zltoolkit)
endif()

#查找openssl是否安装
find_package(OpenSSL QUIET)
if (OPENSSL_FOUND AND ENABLE_OPENSSL)
    message(STATUS "found library:${OPENSSL_LIBRARIES},ENABLE_OPENSSL defined")
    include_directories(${OPENSSL_INCLUDE_DIR})
    add_definitions(-DENABLE_OPENSSL)
    list(APPEND LINK_LIB_LIST ${OPENSSL_LIBRARIES})
endif ()

#查找mysql是否安装
find_package(MYSQL QUIET)
if (MYSQL_FOUND AND ENABLE_MYSQL)
    message(STATUS "found library:${MYSQL_LIBRARIES},ENABLE_MYSQL defined")
    include_directories(${MYSQL_INCLUDE_DIR})
    add_definitions(-DENABLE_MYSQL)
    list(APPEND LINK_LIB_LIST ${MYSQL_LIBRARIES})
endif ()

#查找MP4V2是否安装
find_package(MP4V2 QUIET)
if (MP4V2_FOUND AND ENABLE_MP4V2)
    include_directories(${MP4V2_INCLUDE_DIR})
    list(APPEND LINK_LIB_LIST ${MP4V2_LIBRARY})
    add_definitions(-DENABLE_MP4V2)
    message(STATUS "found library:${MP4V2_LIBRARY},ENABLE_MP4V2 defined")
endif ()

#查找x264是否安装
find_package(X264 QUIET)
if (X264_FOUND AND ENABLE_X264)
    message(STATUS "found library:${X264_LIBRARIES},ENABLE_X264 defined")
    include_directories(${X264_INCLUDE_DIRS})
    add_definitions(-DENABLE_X264)
    list(APPEND LINK_LIB_LIST ${X264_LIBRARIES})
endif ()

#查找faac是否安装
find_package(FAAC QUIET)
if (FAAC_FOUND AND ENABLE_FAAC)
    message(STATUS "found library:${FAAC_LIBRARIES},ENABLE_FAAC defined")
    include_directories(${FAAC_INCLUDE_DIR})
    add_definitions(-DENABLE_FAAC)
    list(APPEND LINK_LIB_LIST ${FAAC_LIBRARIES})
endif ()


#libmpeg
if(ENABLE_HLS)
    aux_source_directory(${MediaServer_Root}/libmpeg/include src_mpeg)
    aux_source_directory(${MediaServer_Root}/libmpeg/source src_mpeg)
    include_directories(${MediaServer_Root}/libmpeg/include)
    add_library(mpeg STATIC ${src_mpeg})
endif()


#添加库
add_library(zltoolkit STATIC ${ToolKit_src_list})
add_library(zlmediakit STATIC ${MediaKit_src_list})
add_library(zlmediakit_jni SHARED ${JNI_src_list})

#链接
target_link_libraries(zlmediakit_jni ${LINK_LIB_LIST} log z)