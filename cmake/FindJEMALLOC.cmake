

find_path(JEMALLOC_INCLUDE_DIR
		NAMES jemalloc/jemalloc.h
		)

find_library(JEMALLOC_LIBRARY
		NAMES jemalloc
		)

set(JEMALLOC_INCLUDE_DIRS ${JEMALLOC_INCLUDE_DIR})
set(JEMALLOC_LIBRARIES ${JEMALLOC_LIBRARY})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(JEMALLOC DEFAULT_MSG JEMALLOC_LIBRARY JEMALLOC_INCLUDE_DIR)
