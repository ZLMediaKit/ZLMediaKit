# Download and build a local static BoringSSL bundle for QUIC-capable builds.

set(ZLM_BORINGSSL_NAME boringssl)
set(ZLM_BORINGSSL_GIT_URL https://github.com/google/boringssl.git)
set(ZLM_BORINGSSL_SOURCE_DIR ${DEP_ROOT_DIR}/${ZLM_BORINGSSL_NAME})
set(ZLM_BORINGSSL_BUILD_DIR ${ZLM_BORINGSSL_SOURCE_DIR}/build)
set(ZLM_BORINGSSL_INCLUDE_DIR ${ZLM_BORINGSSL_SOURCE_DIR}/include)
set(ZLM_BORINGSSL_SSL_LIBRARY ${ZLM_BORINGSSL_BUILD_DIR}/ssl/libssl.a)
set(ZLM_BORINGSSL_CRYPTO_LIBRARY ${ZLM_BORINGSSL_BUILD_DIR}/crypto/libcrypto.a)

function(zlm_boringssl_run_checked)
  execute_process(${ARGN} RESULT_VARIABLE _zlm_result)
  if(NOT _zlm_result EQUAL 0)
    message(FATAL_ERROR "Command failed: ${ARGN}")
  endif()
endfunction()

if(WIN32)
  message(FATAL_ERROR "Automatic BoringSSL build for QUIC is only implemented for Unix-like platforms")
endif()

if(NOT GIT_FOUND)
  find_package(Git QUIET)
endif()
if(NOT GIT_FOUND)
  message(FATAL_ERROR "ENABLE_LSQUIC_AUTO_BUILD requires Git")
endif()

include(ZLMGo)

find_program(ZLM_NINJA_EXECUTABLE ninja)
if(NOT ZLM_NINJA_EXECUTABLE)
  message(FATAL_ERROR "ENABLE_LSQUIC_AUTO_BUILD requires ninja to build BoringSSL")
endif()

set(ZLM_GO_CACHE_DIR ${DEP_ROOT_DIR}/go-build-cache)
file(MAKE_DIRECTORY ${ZLM_GO_CACHE_DIR})

if(EXISTS ${ZLM_BORINGSSL_SOURCE_DIR}/.git AND NOT EXISTS ${ZLM_BORINGSSL_SOURCE_DIR}/CMakeLists.txt)
  file(REMOVE_RECURSE ${ZLM_BORINGSSL_SOURCE_DIR})
endif()

if(NOT EXISTS ${ZLM_BORINGSSL_SOURCE_DIR}/.git)
  message(STATUS "Initializing BoringSSL checkout in ${ZLM_BORINGSSL_SOURCE_DIR}")
  file(MAKE_DIRECTORY ${ZLM_BORINGSSL_SOURCE_DIR})
  zlm_boringssl_run_checked(
    COMMAND ${GIT_EXECUTABLE} -C ${ZLM_BORINGSSL_SOURCE_DIR} init)
  zlm_boringssl_run_checked(
    COMMAND ${GIT_EXECUTABLE} -C ${ZLM_BORINGSSL_SOURCE_DIR} remote add origin ${ZLM_BORINGSSL_GIT_URL})
endif()

if(DEFINED BORINGSSL_REVISION AND BORINGSSL_REVISION)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} -C ${ZLM_BORINGSSL_SOURCE_DIR} rev-parse HEAD
    RESULT_VARIABLE ZLM_BORINGSSL_HEAD_RESULT
    OUTPUT_VARIABLE ZLM_BORINGSSL_HEAD
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(NOT ZLM_BORINGSSL_HEAD_RESULT EQUAL 0 OR NOT ZLM_BORINGSSL_HEAD STREQUAL BORINGSSL_REVISION)
    message(STATUS "Checking out BoringSSL revision ${BORINGSSL_REVISION}")
    zlm_boringssl_run_checked(
      COMMAND ${GIT_EXECUTABLE} -C ${ZLM_BORINGSSL_SOURCE_DIR} fetch --depth 1 origin ${BORINGSSL_REVISION})
    zlm_boringssl_run_checked(
      COMMAND ${GIT_EXECUTABLE} -C ${ZLM_BORINGSSL_SOURCE_DIR} checkout --force FETCH_HEAD)
  endif()
endif()

if(NOT EXISTS ${ZLM_BORINGSSL_BUILD_DIR}/build.ninja)
  message(STATUS "Configuring local static BoringSSL")
  zlm_boringssl_run_checked(
    COMMAND ${CMAKE_COMMAND} -E env
      GO111MODULE=off
      GOCACHE=${ZLM_GO_CACHE_DIR}
      GOROOT=${ZLM_GO_ROOT_DIR}
      PATH=${ZLM_GO_ROOT_DIR}/bin:$ENV{PATH}
      ${CMAKE_COMMAND}
      -S ${ZLM_BORINGSSL_SOURCE_DIR}
      -B ${ZLM_BORINGSSL_BUILD_DIR}
      -G Ninja
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON
      -DGO_EXECUTABLE=${ZLM_GO_EXECUTABLE}
      -DBUILD_SHARED_LIBS=OFF)
endif()

if(NOT EXISTS ${ZLM_BORINGSSL_SSL_LIBRARY} OR NOT EXISTS ${ZLM_BORINGSSL_CRYPTO_LIBRARY})
  message(STATUS "Building local static BoringSSL")
  zlm_boringssl_run_checked(
    COMMAND ${CMAKE_COMMAND} -E env
      GO111MODULE=off
      GOCACHE=${ZLM_GO_CACHE_DIR}
      GOROOT=${ZLM_GO_ROOT_DIR}
      PATH=${ZLM_GO_ROOT_DIR}/bin:$ENV{PATH}
      ${CMAKE_COMMAND} --build ${ZLM_BORINGSSL_BUILD_DIR} --target ssl crypto)
endif()

set(BORINGSSL_ROOT_DIR ${ZLM_BORINGSSL_SOURCE_DIR} CACHE PATH "Local BoringSSL root directory" FORCE)
set(BORINGSSL_INCLUDE ${ZLM_BORINGSSL_INCLUDE_DIR} CACHE PATH "BoringSSL include directory" FORCE)
set(BORINGSSL_LIB_ssl ${ZLM_BORINGSSL_SSL_LIBRARY} CACHE FILEPATH "BoringSSL ssl library path" FORCE)
set(BORINGSSL_LIB_crypto ${ZLM_BORINGSSL_CRYPTO_LIBRARY} CACHE FILEPATH "BoringSSL crypto library path" FORCE)
