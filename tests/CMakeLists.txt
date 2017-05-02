aux_source_directory(. TEST_SRC_LIST)
foreach(TEST_SRC ${TEST_SRC_LIST})
STRING(REGEX REPLACE ".cpp" "" TEST_EXE_NAME ${TEST_SRC})
STRING(REGEX REPLACE "./" "" TEST_EXE_NAME ${TEST_EXE_NAME})
message(STATUS "add test program:${TEST_EXE_NAME}")
add_executable(${TEST_EXE_NAME} ${TEST_SRC})
target_link_libraries(${TEST_EXE_NAME} ${CMAKE_PROJECT_NAME}_shared pthread)

#查找mp4v2是否安装
if(MP4V2_FOUND)
target_link_libraries(${TEST_EXE_NAME} mp4v2)
endif(MP4V2_FOUND)

#查找x264是否安装
if(X264_FOUND)
target_link_libraries(${TEST_EXE_NAME} x264)
endif(X264_FOUND)

#查找faac是否安装
if(FAAC_FOUND)
target_link_libraries(${TEST_EXE_NAME} faac)
endif(FAAC_FOUND)

#查找ZLToolKit是否安装
if(ZLTOOLKIT_FOUND)
target_link_libraries(${TEST_EXE_NAME} ZLToolKit)
endif(ZLTOOLKIT_FOUND)

#查找openssl是否安装
if(OPENSSL_FOUND)
target_link_libraries(${TEST_EXE_NAME} ssl)
endif(OPENSSL_FOUND)

#查找mysql是否安装
if(MYSQL_FOUND)
target_link_libraries(${TEST_EXE_NAME} mysqlclient)
endif(MYSQL_FOUND)

endforeach(TEST_SRC ${TEST_SRC_LIST})
