find_path(SWRESAMPLE_INCLUDE_DIR
        NAMES libswresample/swresample.h
        HINTS ${FFMPEG_PATH_ROOT}
        PATH_SUFFIXES include)

find_library(SWRESAMPLE_LIBRARY
        NAMES swresample
        HINTS ${FFMPEG_PATH_ROOT}
        PATH_SUFFIXES bin lib)

set(SWRESAMPLE_LIBRARIES ${SWRESAMPLE_LIBRARY})
set(SWRESAMPLE_INCLUDE_DIRS ${SWRESAMPLE_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(SWRESAMPLE DEFAULT_MSG SWRESAMPLE_LIBRARY SWRESAMPLE_INCLUDE_DIR)
