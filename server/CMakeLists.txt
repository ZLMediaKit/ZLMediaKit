include_directories(../3rdpart)
file(GLOB jsoncpp_src_list ../3rdpart/jsoncpp/*.cpp ../3rdpart/jsoncpp/*.h )
add_library(jsoncpp STATIC ${jsoncpp_src_list})

file(GLOB MediaServer_src_list ./*.cpp ./*.h)
add_executable(MediaServer ${MediaServer_src_list})
target_link_libraries(MediaServer jsoncpp ${LINK_LIB_LIST})


