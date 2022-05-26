include_directories(include source)
include(GenerateExportHeader)
file(GLOB api_src_list include/*.h source/*.cpp source/*.h source/*.c)

if (IOS)
    add_library(mk_api STATIC ${api_src_list})
    target_link_libraries(mk_api ${LINK_LIB_LIST} jsoncpp)
else ()
    if (ENABLE_API_STATIC_LIB)
        add_library(mk_api STATIC ${api_src_list})
    else ()
        add_library(mk_api SHARED ${api_src_list})
    endif ()

    if (CMAKE_SYSTEM_NAME MATCHES "Linux")
        target_link_libraries(mk_api -Wl,--start-group jsoncpp ${LINK_LIB_LIST} -Wl,--end-group)
    else ()
        target_link_libraries(mk_api jsoncpp ${LINK_LIB_LIST})
    endif ()
    add_subdirectory(tests)
endif ()

add_definitions(-DGENERATE_EXPORT)
include_directories(${CMAKE_BINARY_DIR})
generate_export_header(mk_api
        EXPORT_MACRO_NAME API_EXPORT
        BASE_NAME MK_API
        STATIC_DEFINE MediaKitApi_STATIC
        EXPORT_FILE_NAME "${CMAKE_BINARY_DIR}/mk_export.h"
        )

if (NOT IOS)
    file(GLOB api_header_list include/*.h ${CMAKE_BINARY_DIR}/*.h)
    install(FILES ${api_header_list} DESTINATION ${INSTALL_PATH_INCLUDE})
    install(TARGETS mk_api ARCHIVE DESTINATION ${INSTALL_PATH_LIB} LIBRARY DESTINATION ${INSTALL_PATH_LIB})
endif ()