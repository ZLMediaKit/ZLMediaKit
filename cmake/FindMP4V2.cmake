find_path(MP4V2_INCLUDE_DIR
  NAMES mp4v2/mp4v2.h)

find_library(MP4V2_LIBRARY
  NAMES mp4v2)

set(MP4V2_LIBRARIES ${MP4V2_LIBRARY})
set(MP4V2_INCLUDE_DIRS ${MP4V2_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

# handle the QUIETLY and REQUIRED arguments and set MP4V2_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(MP4V2 DEFAULT_MSG MP4V2_LIBRARY MP4V2_INCLUDE_DIR)
