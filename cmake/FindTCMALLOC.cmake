find_path(Tcmalloc_INCLUDE_DIR
  NAMES google/tcmalloc.h
)

find_library(Tcmalloc_LIBRARY
  NAMES tcmalloc_minimal tcmalloc
)

set(TCMALLOC_LIBRARIES ${Tcmalloc_LIBRARY})
set(TCMALLOC_INCLUDE_DIRS ${Tcmalloc_INCLUDE_DIR})

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TCMALLOC
  DEFAULT_MSG
  TCMALLOC_LIBRARIES TCMALLOC_INCLUDE_DIRS
)
