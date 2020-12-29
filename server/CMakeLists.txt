include_directories(../3rdpart)
file(GLOB jsoncpp_src_list ../3rdpart/jsoncpp/*.cpp ../3rdpart/jsoncpp/*.h )
add_library(jsoncpp STATIC ${jsoncpp_src_list})

file(GLOB MediaServer_src_list ./*.cpp ./*.h)
#message(STATUS ${MediaServer_src_list})

# 添加git版本信息
set(COMMIT_HASH "Git_NotFound_Unkown_commit")
set(BRANCH_NAME "Git_NotFound_Unkown_branch")
set(BUILD_TIME "")

string(TIMESTAMP BUILD_TIME "%Y/%m/%d-%H:%M:%S")

find_package(Git QUIET)
if(GIT_FOUND)
	execute_process(
			COMMAND ${GIT_EXECUTABLE} log -1 --pretty=format:%H
			OUTPUT_VARIABLE COMMIT_HASH
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../
	)
	execute_process(
			COMMAND ${GIT_EXECUTABLE} symbolic-ref --short -q HEAD
			OUTPUT_VARIABLE BRANCH_NAME
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../
	)
endif()

message(STATUS "Git version is ${BRANCH_NAME}:${COMMIT_HASH}:${BUILD_TIME}")

configure_file(
		${CMAKE_CURRENT_SOURCE_DIR}/version.h.ini
		${CMAKE_BINARY_DIR}/Version.h
		@ONLY
)
include_directories(${CMAKE_BINARY_DIR})
add_definitions(-DENABLE_VERSION)

add_executable(MediaServer ${MediaServer_src_list})

if(WIN32)
	set_target_properties(MediaServer PROPERTIES COMPILE_FLAGS  ${VS_FALGS} )
else()
	install(TARGETS MediaServer DESTINATION ${INSTALL_PATH_EXECUTABLE})
endif()

target_link_libraries(MediaServer jsoncpp ${LINK_LIB_LIST})
