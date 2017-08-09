# - Try to find MySQL / MySQL Embedded library
# Find the MySQL includes and client library
# This module defines
#  MYSQL_INCLUDE_DIR, where to find mysql.h
#  MYSQL_LIBRARIES, the libraries needed to use MySQL.
#  MYSQL_LIB_DIR, path to the MYSQL_LIBRARIES
#  MYSQL_EMBEDDED_LIBRARIES, the libraries needed to use MySQL Embedded.
#  MYSQL_EMBEDDED_LIB_DIR, path to the MYSQL_EMBEDDED_LIBRARIES
#  MYSQL_FOUND, If false, do not try to use MySQL.
#  MYSQL_EMBEDDED_FOUND, If false, do not try to use MySQL Embedded.

# Copyright (c) 2006-2008, Jarosław Staniek <staniek@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

include(CheckCXXSourceCompiles)

if(WIN32)
   find_path(MYSQL_INCLUDE_DIR mysql.h
      PATHS
      $ENV{MYSQL_INCLUDE_DIR}
      $ENV{MYSQL_DIR}/include
      $ENV{ProgramFiles}/MySQL/*/include
      $ENV{SystemDrive}/MySQL/*/include
      $ENV{ProgramW6432}/MySQL/*/include
   )
else(WIN32)
   find_path(MYSQL_INCLUDE_DIR mysql/mysql.h
      PATHS
      $ENV{MYSQL_INCLUDE_DIR}
      $ENV{MYSQL_DIR}/include
      /usr/local/mysql/include
      /opt/mysql/mysql/include
      PATH_SUFFIXES
      mysql
   )
endif(WIN32)

if(WIN32)
   if (${CMAKE_BUILD_TYPE})
    string(TOLOWER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_TOLOWER)
   endif()

   # path suffix for debug/release mode
   # binary_dist: mysql binary distribution
   # build_dist: custom build
   if(CMAKE_BUILD_TYPE_TOLOWER MATCHES "debug")
      set(binary_dist debug)
      set(build_dist Debug)
   else(CMAKE_BUILD_TYPE_TOLOWER MATCHES "debug")
      ADD_DEFINITIONS(-DDBUG_OFF)
      set(binary_dist opt)
      set(build_dist Release)
   endif(CMAKE_BUILD_TYPE_TOLOWER MATCHES "debug")

#   find_library(MYSQL_LIBRARIES NAMES mysqlclient
   set(MYSQL_LIB_PATHS
      $ENV{MYSQL_DIR}/lib/${binary_dist}
      $ENV{MYSQL_DIR}/libmysql/${build_dist}
      $ENV{MYSQL_DIR}/client/${build_dist}
      $ENV{ProgramFiles}/MySQL/*/lib/${binary_dist}
      $ENV{SystemDrive}/MySQL/*/lib/${binary_dist}
      $ENV{MYSQL_DIR}/lib/opt
      $ENV{MYSQL_DIR}/client/release
      $ENV{ProgramFiles}/MySQL/*/lib/opt
	  $ENV{ProgramFiles}/MySQL/*/lib/
      $ENV{SystemDrive}/MySQL/*/lib/opt
      $ENV{ProgramW6432}/MySQL/*/lib
   )
   find_library(MYSQL_LIBRARIES NAMES libmysql
      PATHS
      ${MYSQL_LIB_PATHS}
   )
else(WIN32)
#   find_library(MYSQL_LIBRARIES NAMES mysqlclient
   set(MYSQL_LIB_PATHS
      $ENV{MYSQL_DIR}/libmysql_r/.libs
      $ENV{MYSQL_DIR}/lib
      $ENV{MYSQL_DIR}/lib/mysql
      /usr/local/mysql/lib
      /opt/mysql/mysql/lib
      $ENV{MYSQL_DIR}/libmysql_r/.libs
      $ENV{MYSQL_DIR}/lib
      $ENV{MYSQL_DIR}/lib/mysql
      /usr/local/mysql/lib
      /opt/mysql/mysql/lib
      PATH_SUFFIXES
      mysql
   )
   find_library(MYSQL_LIBRARIES NAMES mysqlclient
      PATHS
      ${MYSQL_LIB_PATHS}
   )
endif(WIN32)

find_library(MYSQL_EMBEDDED_LIBRARIES NAMES mysqld
   PATHS
   ${MYSQL_LIB_PATHS}
)

if(MYSQL_LIBRARIES)
   get_filename_component(MYSQL_LIB_DIR ${MYSQL_LIBRARIES} PATH)
endif(MYSQL_LIBRARIES)

if(MYSQL_EMBEDDED_LIBRARIES)
   get_filename_component(MYSQL_EMBEDDED_LIB_DIR ${MYSQL_EMBEDDED_LIBRARIES} PATH)
endif(MYSQL_EMBEDDED_LIBRARIES)

set( CMAKE_REQUIRED_INCLUDES ${MYSQL_INCLUDE_DIR} )
set( CMAKE_REQUIRED_LIBRARIES ${MYSQL_EMBEDDED_LIBRARIES} )
check_cxx_source_compiles( "#include <mysql.h>\nint main() { int i = MYSQL_OPT_USE_EMBEDDED_CONNECTION; }" HAVE_MYSQL_OPT_EMBEDDED_CONNECTION )

if(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARIES)
   set(MYSQL_FOUND TRUE)
   message(STATUS "Found MySQL: ${MYSQL_INCLUDE_DIR}, ${MYSQL_LIBRARIES}")
else(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARIES)
   set(MYSQL_FOUND FALSE)
   message(STATUS "MySQL not found.")
endif(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARIES)

if(MYSQL_INCLUDE_DIR AND MYSQL_EMBEDDED_LIBRARIES AND HAVE_MYSQL_OPT_EMBEDDED_CONNECTION)
   set(MYSQL_EMBEDDED_FOUND TRUE)
   message(STATUS "Found MySQL Embedded: ${MYSQL_INCLUDE_DIR}, ${MYSQL_EMBEDDED_LIBRARIES}")
else(MYSQL_INCLUDE_DIR AND MYSQL_EMBEDDED_LIBRARIES AND HAVE_MYSQL_OPT_EMBEDDED_CONNECTION)
   set(MYSQL_EMBEDDED_FOUND FALSE)
   message(STATUS "MySQL Embedded not found.")
endif(MYSQL_INCLUDE_DIR AND MYSQL_EMBEDDED_LIBRARIES AND HAVE_MYSQL_OPT_EMBEDDED_CONNECTION)

mark_as_advanced(MYSQL_INCLUDE_DIR MYSQL_LIBRARIES MYSQL_EMBEDDED_LIBRARIES)
