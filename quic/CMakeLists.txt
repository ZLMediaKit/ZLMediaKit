# MIT License
#
# Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
#

if(ENABLE_LSQUIC_AUTO_BUILD)
  if(NOT EXISTS ${DEP_ROOT_DIR})
    file(MAKE_DIRECTORY ${DEP_ROOT_DIR})
  endif()
  include(ZLMBoringSSL)
endif()

if((NOT DEFINED LSQUIC_ROOT_DIR OR NOT LSQUIC_ROOT_DIR) AND EXISTS "${DEP_ROOT_DIR}/lsquic-${LSQUIC_VERSION}/include/lsquic.h")
  set(LSQUIC_ROOT_DIR "${DEP_ROOT_DIR}/lsquic-${LSQUIC_VERSION}" CACHE PATH "Bundled LSQUIC root directory" FORCE)
endif()
if((NOT DEFINED LSQUIC_INCLUDE_DIR OR NOT LSQUIC_INCLUDE_DIR) AND EXISTS "${DEP_ROOT_DIR}/lsquic-${LSQUIC_VERSION}/include/lsquic.h")
  set(LSQUIC_INCLUDE_DIR "${DEP_ROOT_DIR}/lsquic-${LSQUIC_VERSION}/include" CACHE PATH "Bundled LSQUIC include directory" FORCE)
endif()
if((NOT DEFINED LSQUIC_SSL_LIBRARY OR NOT LSQUIC_SSL_LIBRARY) AND DEFINED BORINGSSL_LIB_ssl AND EXISTS "${BORINGSSL_LIB_ssl}")
  set(LSQUIC_SSL_LIBRARY "${BORINGSSL_LIB_ssl}" CACHE FILEPATH "Bundled BoringSSL ssl library used by LSQUIC" FORCE)
endif()
if((NOT DEFINED LSQUIC_CRYPTO_LIBRARY OR NOT LSQUIC_CRYPTO_LIBRARY) AND DEFINED BORINGSSL_LIB_crypto AND EXISTS "${BORINGSSL_LIB_crypto}")
  set(LSQUIC_CRYPTO_LIBRARY "${BORINGSSL_LIB_crypto}" CACHE FILEPATH "Bundled BoringSSL crypto library used by LSQUIC" FORCE)
endif()
if(NOT DEFINED LSQUIC_LIBRARY OR NOT LSQUIC_LIBRARY)
  if(EXISTS "${DEP_ROOT_DIR}/lsquic-${LSQUIC_VERSION}/build-boringssl/src/liblsquic/liblsquic.a")
    set(LSQUIC_LIBRARY "${DEP_ROOT_DIR}/lsquic-${LSQUIC_VERSION}/build-boringssl/src/liblsquic/liblsquic.a" CACHE FILEPATH "Bundled LSQUIC static library" FORCE)
  elseif(EXISTS "${DEP_ROOT_DIR}/lsquic-${LSQUIC_VERSION}/build-boringssl/src/liblsquic/liblsquic.so")
    set(LSQUIC_LIBRARY "${DEP_ROOT_DIR}/lsquic-${LSQUIC_VERSION}/build-boringssl/src/liblsquic/liblsquic.so" CACHE FILEPATH "Bundled LSQUIC shared library" FORCE)
  elseif(EXISTS "${DEP_ROOT_DIR}/lsquic-${LSQUIC_VERSION}/build/src/liblsquic/liblsquic.a")
    set(LSQUIC_LIBRARY "${DEP_ROOT_DIR}/lsquic-${LSQUIC_VERSION}/build/src/liblsquic/liblsquic.a" CACHE FILEPATH "Bundled LSQUIC static library" FORCE)
  elseif(EXISTS "${DEP_ROOT_DIR}/lsquic-${LSQUIC_VERSION}/build/src/liblsquic/liblsquic.so")
    set(LSQUIC_LIBRARY "${DEP_ROOT_DIR}/lsquic-${LSQUIC_VERSION}/build/src/liblsquic/liblsquic.so" CACHE FILEPATH "Bundled LSQUIC shared library" FORCE)
  endif()
endif()

if(ENABLE_LSQUIC_AUTO_BUILD)
  set(LSQUIC_PREFER_SHARED OFF)
  include(LSQUIC)
else()
  set(LSQUIC_PREFER_SHARED ON)
endif()

find_package(LSQUIC QUIET)
if(NOT LSQUIC_FOUND)
  message(FATAL_ERROR
    "ENABLE_QUIC requires a real LSQUIC dependency bundle. "
    "Set LSQUIC_ROOT_DIR / LSQUIC_LIBRARY explicitly or enable ENABLE_LSQUIC_AUTO_BUILD=ON.")
endif()
message(STATUS "found library: ${LSQUIC_LIBRARIES}, ENABLE_QUIC defined")
update_cached_list(MK_COMPILE_DEFINITIONS ENABLE_QUIC)

set(QUIC_HOST_SRC_LIST
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicDispatcher.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicDispatcher.h
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicClientBackend.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicClientBackend.h
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicSession.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicSession.h
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicBackend.h
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicPlugin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicPluginABI.h
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicPluginBackend.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicPluginBackend.h)

set(QUIC_PLUGIN_SRC_LIST
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicPluginABI.h
  ${CMAKE_CURRENT_SOURCE_DIR}/plugin/lsquic/LsquicCommon.h
  ${CMAKE_CURRENT_SOURCE_DIR}/plugin/lsquic/LsquicCommon.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/plugin/lsquic/LsquicServerEngine.h
  ${CMAKE_CURRENT_SOURCE_DIR}/plugin/lsquic/LsquicServerEngine.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/plugin/lsquic/LsquicClientEngine.h
  ${CMAKE_CURRENT_SOURCE_DIR}/plugin/lsquic/LsquicClientEngine.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/plugin/lsquic/LsquicPlugin.cpp)

set(QUIC_PLUGIN_EXPORT_MAP
  ${CMAKE_CURRENT_SOURCE_DIR}/plugin/lsquic/zlm_quic.map)

add_library(zlm_quic_lsquic SHARED ${QUIC_PLUGIN_SRC_LIST})
add_library(ZLMediaKit::QUICPlugin ALIAS zlm_quic_lsquic)

target_compile_definitions(zlm_quic_lsquic
  PRIVATE
    ZLM_QUIC_PLUGIN_EXPORTS
    ZLM_QUIC_HAS_LSQUIC=1)

target_compile_options(zlm_quic_lsquic
  PRIVATE ${COMPILE_OPTIONS_DEFAULT})

target_include_directories(zlm_quic_lsquic
  PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

target_link_libraries(zlm_quic_lsquic
  PRIVATE
    LSQUIC::LSQUIC)

set_target_properties(zlm_quic_lsquic PROPERTIES
  C_VISIBILITY_PRESET hidden
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES)

if(UNIX AND NOT APPLE)
  set_property(TARGET zlm_quic_lsquic APPEND_STRING PROPERTY LINK_FLAGS
    " -Wl,--exclude-libs,ALL -Wl,--version-script=${QUIC_PLUGIN_EXPORT_MAP}")
endif()

add_library(zlm_quic_host STATIC ${QUIC_HOST_SRC_LIST})
add_library(ZLMediaKit::QUIC ALIAS zlm_quic_host)

target_link_libraries(zlm_quic_host
  PUBLIC
    ZLMediaKit::MediaKit
    ZLMediaKit::ToolKit
    ZLMediaKit::QUICPlugin)

target_compile_options(zlm_quic_host
  PRIVATE ${COMPILE_OPTIONS_DEFAULT})

target_include_directories(zlm_quic_host
  PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>")

update_cached_list(MK_LINK_LIBRARIES ZLMediaKit::QUIC)

message(STATUS "QUIC shared plugin is enabled")
