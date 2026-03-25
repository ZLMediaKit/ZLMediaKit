# MIT License
#
# Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
#

set(QUIC_HOST_SRC_LIST
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicDispatcher.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicDispatcher.h
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicSession.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicSession.h
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicBackend.h
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicPlugin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicPluginABI.h
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicPluginBackend.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicPluginBackend.h)

set(QUIC_PLUGIN_SRC_LIST
  ${CMAKE_CURRENT_SOURCE_DIR}/QuicPluginABI.h
  ${CMAKE_CURRENT_SOURCE_DIR}/plugin/lsquic/LsquicPlugin.cpp)

set(QUIC_PLUGIN_EXPORT_MAP
  ${CMAKE_CURRENT_SOURCE_DIR}/plugin/lsquic/zlm_quic.map)

add_library(zlm_quic_lsquic SHARED ${QUIC_PLUGIN_SRC_LIST})
add_library(ZLMediaKit::QUICPlugin ALIAS zlm_quic_lsquic)

target_compile_definitions(zlm_quic_lsquic
  PRIVATE ZLM_QUIC_PLUGIN_EXPORTS)

target_compile_options(zlm_quic_lsquic
  PRIVATE ${COMPILE_OPTIONS_DEFAULT})

target_include_directories(zlm_quic_lsquic
  PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

if(TARGET LSQUIC::LSQUIC)
  target_link_libraries(zlm_quic_lsquic
    PRIVATE
      LSQUIC::LSQUIC)
  target_compile_definitions(zlm_quic_lsquic PRIVATE ZLM_QUIC_HAS_LSQUIC=1)
else()
  message(STATUS "zlm_quic_lsquic will be built without linking LSQUIC::LSQUIC")
endif()

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

message(STATUS "QUIC shared-plugin skeleton is enabled")
