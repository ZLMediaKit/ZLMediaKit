find_path(SWSCALE_INCLUDE_DIR
        NAMES libswscale/swscale.h
        HINTS ${FFMPEG_PATH_ROOT}
        PATH_SUFFIXES include)

find_library(SWSCALE_LIBRARY
        NAMES swscale
        HINTS ${FFMPEG_PATH_ROOT}
        PATH_SUFFIXES bin lib)

set(SWSCALE_LIBRARIES ${SWSCALE_LIBRARY})
set(SWSCALE_INCLUDE_DIRS ${SWSCALE_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(SWSCALE DEFAULT_MSG SWSCALE_LIBRARY SWSCALE_INCLUDE_DIR)
