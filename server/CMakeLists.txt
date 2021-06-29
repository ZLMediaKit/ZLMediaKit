include_directories(../3rdpart)
file(GLOB jsoncpp_src_list ../3rdpart/jsoncpp/*.cpp ../3rdpart/jsoncpp/*.h )
add_library(jsoncpp STATIC ${jsoncpp_src_list})
file(GLOB MediaServer_src_list ./*.cpp ./*.h)

add_executable(MediaServer ${MediaServer_src_list})

if(MSVC)
	set_target_properties(MediaServer PROPERTIES COMPILE_FLAGS  ${VS_FALGS} )
else()
	install(TARGETS MediaServer DESTINATION ${INSTALL_PATH_EXECUTABLE})
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
	target_link_libraries(MediaServer -Wl,--start-group jsoncpp ${LINK_LIB_LIST} -Wl,--end-group)
else ()
	target_link_libraries(MediaServer jsoncpp ${LINK_LIB_LIST})
endif ()
