# Download and build Jemalloc

set(JEMALLOC_VERSION 5.3.0)
set(JEMALLOC_NAME jemalloc-${JEMALLOC_VERSION})
set(JEMALLOC_TAR_PATH ${DEP_ROOT_DIR}/${JEMALLOC_NAME}.tar.bz2)

list(APPEND jemalloc_CONFIG_ARGS --disable-initial-exec-tls)
#list(APPEND jemalloc_CONFIG_ARGS --without-export)
if (ENABLE_JEMALLOC_STAT)
    list(APPEND jemalloc_CONFIG_ARGS --enable-stats)
    message(STATUS "Jemalloc stats enabled")
else ()
list(APPEND jemalloc_CONFIG_ARGS --disable-stats)
    message(STATUS "Jemalloc stats disabled")
endif ()
list(APPEND jemalloc_CONFIG_ARGS --disable-libdl)
#list(APPEND jemalloc_CONFIG_ARGS --disable-cxx)
#list(APPEND jemalloc_CONFIG_ARGS --with-jemalloc-prefix=je_)
#list(APPEND jemalloc_CONFIG_ARGS --enable-debug)

if(NOT EXISTS ${JEMALLOC_TAR_PATH})
    set(JEMALLOC_URL https://github.com/jemalloc/jemalloc/releases/download/${JEMALLOC_VERSION}/${JEMALLOC_NAME}.tar.bz2)
    message(STATUS "Downloading ${JEMALLOC_NAME} from ${JEMALLOC_URL}")
    file(DOWNLOAD ${JEMALLOC_URL} ${JEMALLOC_TAR_PATH} SHOW_PROGRESS STATUS JEMALLOC_DOWNLOAD_STATUS LOG JEMALLOC_DOWNLOAD_LOG)
    list(GET JEMALLOC_DOWNLOAD_STATUS 0 JEMALLOC_DOWNLOAD_STATUS_CODE)
    if(NOT JEMALLOC_DOWNLOAD_STATUS_CODE EQUAL 0)
        file(REMOVE ${JEMALLOC_TAR_PATH})
        message(STATUS "${JEMALLOC_DOWNLOAD_LOG}")
        message(FATAL_ERROR "${JEMALLOC_NAME} download failed! error is ${JEMALLOC_DOWNLOAD_STATUS}")
        return()
    endif ()
endif()

SET( DIR_CONTAINING_JEMALLOC ${DEP_ROOT_DIR}/${JEMALLOC_NAME} )

if(NOT EXISTS ${DIR_CONTAINING_JEMALLOC})
    message(STATUS "Extracting jemalloc...")
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf ${JEMALLOC_TAR_PATH} WORKING_DIRECTORY ${DEP_ROOT_DIR})
endif()


if(NOT EXISTS ${DIR_CONTAINING_JEMALLOC}/Makefile)
    message("Configuring jemalloc locally...")
    # Builds with "--with-jemalloc-prefix=je_" on OSX
#    SET( BASH_COMMAND_TO_RUN bash -l -c "cd ${DIR_CONTAINING_JEMALLOC} && ./configure ${jemalloc_CONFIG_ARGS}" )
#
#    EXECUTE_PROCESS( COMMAND ${BASH_COMMAND_TO_RUN}
#            WORKING_DIRECTORY ${DIR_CONTAINING_JEMALLOC} RESULT_VARIABLE JEMALLOC_CONFIGURE )

    execute_process(COMMAND ./configure ${jemalloc_CONFIG_ARGS} WORKING_DIRECTORY ${DIR_CONTAINING_JEMALLOC} RESULT_VARIABLE JEMALLOC_CONFIGURE)
    if(NOT JEMALLOC_CONFIGURE EQUAL 0)
        message(FATAL_ERROR "${JEMALLOC_NAME} configure failed!")
        message("${JEMALLOC_CONFIGURE}")
    endif()
endif()

if(NOT EXISTS ${DIR_CONTAINING_JEMALLOC}/lib/libjemalloc.a)
    message("Building jemalloc locally...")
    execute_process(COMMAND make "build_lib_static" WORKING_DIRECTORY ${DIR_CONTAINING_JEMALLOC})
    if(NOT EXISTS ${DIR_CONTAINING_JEMALLOC}/lib/libjemalloc.a)
        message(FATAL_ERROR "${JEMALLOC_NAME} build failed!")
    endif()
endif()