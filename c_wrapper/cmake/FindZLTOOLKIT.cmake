find_path(ZLTOOLKIT_INCLUDE_DIR
  NAMES Network/Socket.h
  PATHS 
  ${PROJECT_SOURCE_DIR}/../ZLToolKit/src
  $ENV{HOME}/ZLToolKit/include)

find_library(ZLTOOLKIT_LIBRARY
  NAMES ZLToolKit
  PATHS 
  ${PROJECT_SOURCE_DIR}/../ZLToolKit/build/lib
  $ENV{HOME}/ZLToolKit/lib)

set(ZLTOOLKIT_LIBRARIES ${ZLTOOLKIT_LIBRARY})
set(ZLTOOLKIT_INCLUDE_DIRS ${ZLTOOLKIT_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(ZLTOOLKIT DEFAULT_MSG ZLTOOLKIT_LIBRARY ZLTOOLKIT_INCLUDE_DIR)
