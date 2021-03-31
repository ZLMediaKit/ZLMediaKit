list(APPEND LINK_LIB_LIST webrtc)
#查找srtp是否安装
find_package(SRTP QUIET)
if (SRTP_FOUND)
	message(STATUS "found library:${SRTP_LIBRARIES}")
	include_directories(${SRTP_INCLUDE_DIRS})
	list(APPEND LINK_LIB_LIST ${SRTP_LIBRARIES})
else ()
	message(FATAL_ERROR "srtp未找到!")
endif ()

include_directories(./)
file(GLOB SRC_LIST ./*.cpp ./*.h ./*.hpp)
add_library(webrtc ${SRC_LIST})
set(LINK_LIB_LIST ${LINK_LIB_LIST} PARENT_SCOPE)

