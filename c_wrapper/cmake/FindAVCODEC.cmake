find_path(AVCODEC_INCLUDE_DIR
  NAMES libavcodec/avcodec.h
  PATHS $ENV{HOME}/ffmpeg/include)

find_library(AVCODEC_LIBRARY
  NAMES avcodec
  PATHS $ENV{HOME}/ffmpeg/lib)

set(AVCODEC_LIBRARIES ${AVCODEC_LIBRARY})
set(AVCODEC_INCLUDE_DIRS ${AVCODEC_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(AVCODEC DEFAULT_MSG AVCODEC_LIBRARY AVCODEC_INCLUDE_DIR)
