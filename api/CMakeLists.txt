include_directories(include source)
file(GLOB api_src_list include/*.h source/*.cpp source/*.h source/*.c)
add_library(api SHARED ${api_src_list})

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    add_compile_options(-fPIC)
endif()

if(WIN32)
    add_definitions(-DMediaKitApi_EXPORTS)
endif()

target_link_libraries(api ${LINK_LIB_LIST})


