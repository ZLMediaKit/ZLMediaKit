find_path(AVUTIL_INCLUDE_DIR
        NAMES libavutil/avutil.h
        HINTS ${FFMPEG_PATH_ROOT}
        PATH_SUFFIXES include)

find_library(AVUTIL_LIBRARY
        NAMES avutil
        HINTS ${FFMPEG_PATH_ROOT}
        PATH_SUFFIXES bin lib)

set(AVUTIL_LIBRARIES ${AVUTIL_LIBRARY})
set(AVUTIL_INCLUDE_DIRS ${AVUTIL_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(AVUTIL DEFAULT_MSG AVUTIL_LIBRARY AVUTIL_INCLUDE_DIR)
