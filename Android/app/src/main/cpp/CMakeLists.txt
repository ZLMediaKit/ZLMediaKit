project(ZLMediaKit)
cmake_minimum_required(VERSION 3.1.3)
#使能c++11
set(CMAKE_CXX_STANDARD 11)

#设置生成的so动态库最后输出的路径
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libs_export/${ANDROID_ABI})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libs_export/${ANDROID_ABI})
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/libs_export/${ANDROID_ABI}/binary)
set(OPENSSL_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/${ANDROID_ABI}")

#由于openssl库编译时未指定-fPIC，到时github action ci编译失败，先屏蔽掉
#set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")
#set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_ROOT_DIR}/libcrypto.a")
#set(OPENSSL_SSL_LIBRARY "${OPENSSL_ROOT_DIR}/libssl.a")

#设置工程源码根目录
set(JNI_Root ${CMAKE_CURRENT_SOURCE_DIR})
set(ZLMediaKit_Root ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../)

#添加主工程cmake
add_subdirectory(${ZLMediaKit_Root} ${EXECUTABLE_OUTPUT_PATH})

#设置include
include_directories(${JNI_Root})
include_directories(${ZLMediaKit_Root}/src)
include_directories(${ZLMediaKit_Root}/srt)
include_directories(${ZLMediaKit_Root}/webrtc)
include_directories(${ZLMediaKit_Root}/server)
include_directories(${ZLMediaKit_Root}/3rdpart)
include_directories(${ZLMediaKit_Root}/3rdpart/media-server)
include_directories(${ZLMediaKit_Root}/3rdpart/ZLToolKit/src)

#收集源代码添加动态库
file(GLOB JNI_src_list ${JNI_Root}/*.cpp ${JNI_Root}/*.h)
add_library(zlmediakit_jni SHARED ${JNI_src_list})

#链接
target_link_libraries(zlmediakit_jni -Wl,--start-group log z ${MK_LINK_LIBRARIES} -Wl,--end-group)

