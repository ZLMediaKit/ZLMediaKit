# Download and build a local static LSQUIC dependency bundle against a
# private BoringSSL installation used only by the QUIC plugin.

set(LSQUIC_SOURCE_DIR ${DEP_ROOT_DIR}/lsquic-${LSQUIC_VERSION})
set(LSQUIC_BUILD_DIR ${LSQUIC_SOURCE_DIR}/build-boringssl)
set(LSQUIC_LIBRARY_PATH ${LSQUIC_BUILD_DIR}/src/liblsquic/liblsquic.a)

function(zlm_run_checked)
  execute_process(${ARGN} RESULT_VARIABLE _zlm_result)
  if(NOT _zlm_result EQUAL 0)
    message(FATAL_ERROR "Command failed: ${ARGN}")
  endif()
endfunction()

if(NOT GIT_FOUND)
  find_package(Git QUIET)
endif()
if(NOT GIT_FOUND)
  message(FATAL_ERROR "ENABLE_LSQUIC_AUTO_BUILD requires Git")
endif()

if(NOT DEFINED BORINGSSL_ROOT_DIR OR NOT BORINGSSL_ROOT_DIR)
  message(FATAL_ERROR "ENABLE_LSQUIC_AUTO_BUILD requires BORINGSSL_ROOT_DIR")
endif()

if(NOT DEFINED BORINGSSL_INCLUDE OR NOT EXISTS "${BORINGSSL_INCLUDE}/openssl/ssl.h")
  message(FATAL_ERROR "ENABLE_LSQUIC_AUTO_BUILD requires BORINGSSL_INCLUDE")
endif()

if(NOT DEFINED BORINGSSL_LIB_ssl OR NOT EXISTS "${BORINGSSL_LIB_ssl}")
  message(FATAL_ERROR "ENABLE_LSQUIC_AUTO_BUILD requires BORINGSSL_LIB_ssl")
endif()

if(NOT DEFINED BORINGSSL_LIB_crypto OR NOT EXISTS "${BORINGSSL_LIB_crypto}")
  message(FATAL_ERROR "ENABLE_LSQUIC_AUTO_BUILD requires BORINGSSL_LIB_crypto")
endif()

if(NOT EXISTS ${LSQUIC_SOURCE_DIR}/.git)
  message(STATUS "Cloning LSQUIC ${LSQUIC_VERSION} into ${LSQUIC_SOURCE_DIR}")
  zlm_run_checked(
    COMMAND ${GIT_EXECUTABLE} clone --branch ${LSQUIC_VERSION} --depth 1 https://github.com/litespeedtech/lsquic.git ${LSQUIC_SOURCE_DIR})
endif()

zlm_run_checked(
  COMMAND ${GIT_EXECUTABLE} -C ${LSQUIC_SOURCE_DIR} submodule update --init)

if(NOT EXISTS ${LSQUIC_LIBRARY_PATH})
  message(STATUS "Building local static LSQUIC against BoringSSL")
  zlm_run_checked(
    COMMAND ${CMAKE_COMMAND}
      -S ${LSQUIC_SOURCE_DIR}
      -B ${LSQUIC_BUILD_DIR}
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON
      -DLSQUIC_SHARED_LIB=OFF
      -DLSQUIC_BIN=OFF
      -DLSQUIC_TESTS=OFF
      -DLSQUIC_LIBSSL=BORINGSSL
      -DBORINGSSL_DIR=${BORINGSSL_ROOT_DIR}
      -DBORINGSSL_INCLUDE=${BORINGSSL_INCLUDE}
      -DBORINGSSL_LIB_ssl=${BORINGSSL_LIB_ssl}
      -DBORINGSSL_LIB_crypto=${BORINGSSL_LIB_crypto})
  zlm_run_checked(
    COMMAND ${CMAKE_COMMAND} --build ${LSQUIC_BUILD_DIR} --target lsquic)
endif()

set(LSQUIC_ROOT_DIR "${LSQUIC_SOURCE_DIR}" CACHE PATH "Local LSQUIC root directory" FORCE)
set(LSQUIC_INCLUDE_DIR "${LSQUIC_SOURCE_DIR}/include" CACHE PATH "LSQUIC include directory" FORCE)
set(LSQUIC_LIBRARY "${LSQUIC_LIBRARY_PATH}" CACHE FILEPATH "LSQUIC static library path" FORCE)
set(LSQUIC_SSL_LIBRARY "${BORINGSSL_LIB_ssl}" CACHE FILEPATH "BoringSSL ssl library path used by LSQUIC" FORCE)
set(LSQUIC_CRYPTO_LIBRARY "${BORINGSSL_LIB_crypto}" CACHE FILEPATH "BoringSSL crypto library path used by LSQUIC" FORCE)
