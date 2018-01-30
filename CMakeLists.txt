project(ZLMediaKit)
cmake_minimum_required(VERSION 2.8)

#加载自定义模块
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")
#设置库文件路径
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
#设置可执行程序路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
#设置子目录
set(SUB_DIR_LIST "Codec" "Common" "Device" "H264" "Http" "MediaFile" "Player" "Rtmp" "RTP" "Rtsp" "Shell")

#使能GOP缓存
add_definitions(-DENABLE_RING_USEBUF)


#安装目录
if(WIN32)
    set(INSTALL_PATH_LIB $ENV{HOME}/${CMAKE_PROJECT_NAME}/lib)
    set(INSTALL_PATH_INCLUDE $ENV{HOME}/${CMAKE_PROJECT_NAME}/include)
else()
    set(INSTALL_PATH_LIB lib)
    set(INSTALL_PATH_INCLUDE include)
endif()


foreach(SUB_DIR ${SUB_DIR_LIST})
    #遍历源文件
    aux_source_directory(src/${SUB_DIR} SRC_LIST)
    #安装头文件至系统目录
    install(DIRECTORY src/${SUB_DIR} DESTINATION ${INSTALL_PATH_INCLUDE} FILES_MATCHING PATTERN "*.h")
endforeach()

set(LINK_LIB_LIST)

#查找openssl是否安装
find_package(OpenSSL QUIET)
if(OPENSSL_FOUND)
    message(STATUS "找到openssl库:\"${OPENSSL_INCLUDE_DIR}\",ENABLE_OPENSSL宏已打开")
    include_directories(${OPENSSL_INCLUDE_DIR})
    add_definitions(-DENABLE_OPENSSL)
    list(APPEND LINK_LIB_LIST ${OPENSSL_LIBRARIES})
endif()

#查找mysql是否安装
find_package(MYSQL QUIET)
if(MYSQL_FOUND)
    message(STATUS "找到mysqlclient库:\"${MYSQL_INCLUDE_DIR}\",ENABLE_MYSQL宏已打开")
    include_directories(${MYSQL_INCLUDE_DIR})
    add_definitions(-DENABLE_MYSQL)
    list(APPEND LINK_LIB_LIST ${MYSQL_LIBRARIES})
endif()


#查找mp4v2是否安装
find_package(MP4V2 QUIET)
if(MP4V2_FOUND)
    message(STATUS "找到mp4v2库:\"${MP4V2_INCLUDE_DIR}\",ENABLE_MP4V2宏已打开")
    include_directories(${MP4V2_INCLUDE_DIR})
    add_definitions(-DENABLE_MP4V2)
    list(APPEND LINK_LIB_LIST ${MP4V2_LIBRARIES})
endif()

#查找x264是否安装
find_package(X264 QUIET)
if(X264_FOUND)
    message(STATUS "找到x264库:\"${X264_INCLUDE_DIRS}\",ENABLE_X264宏已打开")
    include_directories(${X264_INCLUDE_DIRS})
    add_definitions(-DENABLE_X264)
    list(APPEND LINK_LIB_LIST ${X264_LIBRARIES})
endif()

#查找faac是否安装
find_package(FAAC QUIET)
if(FAAC_FOUND)
    message(STATUS "找到faac库:\"${FAAC_INCLUDE_DIR}\",ENABLE_FAAC宏已打开")
    include_directories(${FAAC_INCLUDE_DIR})
    add_definitions(-DENABLE_FAAC)
    list(APPEND LINK_LIB_LIST ${FAAC_LIBRARIES})
endif()

#查找ZLToolKit是否安装
find_package(ZLTOOLKIT QUIET)
if(ZLTOOLKIT_FOUND)
    message(STATUS "找到ZLToolKit库:\"${ZLTOOLKIT_INCLUDE_DIR}\"")
    include_directories(${ZLTOOLKIT_INCLUDE_DIR})
    list(APPEND LINK_LIB_LIST ${ZLTOOLKIT_LIBRARIES})
endif()

#查找ZLToolKit是否安装
find_package(JEMALLOC QUIET)
if(JEMALLOC_FOUND)
    message(STATUS "找到JEMALLOC库:\"${JEMALLOC_INCLUDE_DIR}\"")
    include_directories(${JEMALLOC_INCLUDE_DIR})
    list(APPEND LINK_LIB_LIST ${JEMALLOC_LIBRARIES})
endif()

#打印库文件
message(STATUS "将链接依赖库:${LINK_LIB_LIST}")
#开启RTSP/RTMP之间的互相转换,开启HLS
add_definitions(-DENABLE_RTMP2RTSP -DENABLE_RTSP2RTMP -DENABLE_HLS)

#引用头文件路径
include_directories(${PROJECT_SOURCE_DIR}/src)

#使能c++11
set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")

if(NOT WIN32)
    #关闭过期接口警告
    add_compile_options(-Wno-deprecated-declarations)
    #关闭__FUNCTION__宏在函数外警告
    add_compile_options(-Wno-predefined-identifier-outside-function)
endif()

#编译动态库
if(NOT IOS AND NOT ANDROID)
    add_library(${CMAKE_PROJECT_NAME}_shared SHARED ${SRC_LIST})
    set_target_properties(${CMAKE_PROJECT_NAME}_shared PROPERTIES OUTPUT_NAME "${CMAKE_PROJECT_NAME}")
    install(TARGETS ${CMAKE_PROJECT_NAME}_shared  ARCHIVE DESTINATION ${INSTALL_PATH_LIB} LIBRARY DESTINATION ${INSTALL_PATH_LIB})
    target_link_libraries(${CMAKE_PROJECT_NAME}_shared ${LINK_LIB_LIST})
endif()

#编译静态库
add_library(${CMAKE_PROJECT_NAME}_static STATIC ${SRC_LIST})
set_target_properties(${CMAKE_PROJECT_NAME}_static PROPERTIES OUTPUT_NAME "${CMAKE_PROJECT_NAME}")
install(TARGETS ${CMAKE_PROJECT_NAME}_static ARCHIVE DESTINATION ${INSTALL_PATH_LIB})

#测试程序
if(NOT IOS)
    add_subdirectory(tests)
endif()



























