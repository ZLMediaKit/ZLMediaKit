find_path(AVFORMAT_INCLUDE_DIR
        NAMES libavformat/avformat.h
        HINTS ${FFMPEG_PATH_ROOT}
        PATH_SUFFIXES include)

find_library(AVFORMAT_LIBRARY
        NAMES avformat
        HINTS ${FFMPEG_PATH_ROOT}
        PATH_SUFFIXES bin lib)

set(AVFORMAT_LIBRARIES ${AVFORMAT_LIBRARY})
set(AVFORMAT_INCLUDE_DIRS ${AVFORMAT_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(AVFORMAT DEFAULT_MSG AVFORMAT_LIBRARY AVFORMAT_INCLUDE_DIR)
