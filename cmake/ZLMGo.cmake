# Download a local Go toolchain used by the QUIC dependency build when the
# host Go installation is incomplete.

set(ZLM_GO_BOOTSTRAP_SUPPORTED TRUE)

set(ZLM_GO_NAME go${GO_AUTO_VERSION}.linux-amd64)
set(ZLM_GO_URL https://go.dev/dl/${ZLM_GO_NAME}.tar.gz)
set(ZLM_GO_TAR_PATH ${DEP_ROOT_DIR}/${ZLM_GO_NAME}.tar.gz)
set(ZLM_GO_ROOT_DIR ${DEP_ROOT_DIR}/go-${GO_AUTO_VERSION})
set(ZLM_GO_EXTRACT_DIR ${DEP_ROOT_DIR}/${ZLM_GO_NAME}-extract)
set(ZLM_GO_EXECUTABLE ${ZLM_GO_ROOT_DIR}/bin/go)

function(zlm_go_run_checked)
  execute_process(${ARGN} RESULT_VARIABLE _zlm_result)
  if(NOT _zlm_result EQUAL 0)
    message(FATAL_ERROR "Command failed: ${ARGN}")
  endif()
endfunction()

if(WIN32 OR APPLE OR NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
  set(ZLM_GO_BOOTSTRAP_SUPPORTED FALSE)
  message(STATUS "Automatic Go toolchain bootstrap is only implemented for Linux x86_64")
  return()
endif()

if(NOT EXISTS ${ZLM_GO_TAR_PATH})
  message(STATUS "Downloading ${ZLM_GO_NAME} from ${ZLM_GO_URL}")
  file(DOWNLOAD ${ZLM_GO_URL} ${ZLM_GO_TAR_PATH}
    SHOW_PROGRESS
    STATUS ZLM_GO_DOWNLOAD_STATUS
    LOG ZLM_GO_DOWNLOAD_LOG)
  list(GET ZLM_GO_DOWNLOAD_STATUS 0 ZLM_GO_DOWNLOAD_STATUS_CODE)
  if(NOT ZLM_GO_DOWNLOAD_STATUS_CODE EQUAL 0)
    file(REMOVE ${ZLM_GO_TAR_PATH})
    message(STATUS "${ZLM_GO_DOWNLOAD_LOG}")
    message(FATAL_ERROR "${ZLM_GO_NAME} download failed: ${ZLM_GO_DOWNLOAD_STATUS}")
  endif()
endif()

if(NOT EXISTS ${ZLM_GO_EXECUTABLE})
  file(REMOVE_RECURSE ${ZLM_GO_EXTRACT_DIR})
  file(MAKE_DIRECTORY ${ZLM_GO_EXTRACT_DIR})
  message(STATUS "Extracting ${ZLM_GO_NAME}")
  zlm_go_run_checked(
    COMMAND ${CMAKE_COMMAND} -E tar xzf ${ZLM_GO_TAR_PATH}
    WORKING_DIRECTORY ${ZLM_GO_EXTRACT_DIR})
  if(NOT EXISTS ${ZLM_GO_EXTRACT_DIR}/go/bin/go)
    message(FATAL_ERROR "Unexpected Go archive layout in ${ZLM_GO_TAR_PATH}")
  endif()
  file(REMOVE_RECURSE ${ZLM_GO_ROOT_DIR})
  file(RENAME ${ZLM_GO_EXTRACT_DIR}/go ${ZLM_GO_ROOT_DIR})
endif()

set(ZLM_GO_ROOT_DIR ${ZLM_GO_ROOT_DIR} CACHE PATH "Local Go root directory" FORCE)
set(ZLM_GO_EXECUTABLE ${ZLM_GO_EXECUTABLE} CACHE FILEPATH "Local Go executable path" FORCE)
