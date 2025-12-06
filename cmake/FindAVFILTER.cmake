find_path(AVFILTER_INCLUDE_DIR
        NAMES libavfilter/avfilter.h
        HINTS ${FFMPEG_PATH_ROOT}
        PATH_SUFFIXES include)

find_library(AVFILTER_LIBRARY
        NAMES avfilter
        HINTS ${FFMPEG_PATH_ROOT}
        PATH_SUFFIXES bin lib)

set(AVFILTER_LIBRARIES ${AVFILTER_LIBRARY})
set(AVFILTER_INCLUDE_DIRS ${AVFILTER_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(AVFILTER DEFAULT_MSG AVFILTER_LIBRARY AVFILTER_INCLUDE_DIR)
