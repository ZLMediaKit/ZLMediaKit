find_path(SDL2_INCLUDE_DIR
	NAMES SDL2/SDL.h
	HINTS SDL2
	PATHS $ENV{HOME}/sdl2/include)

find_library(SDL2_LIBRARY
	NAMES SDL2
	PATHS $ENV{HOME}/sdl2/lib/x86)

set(SDL2_LIBRARIES ${SDL2_LIBRARY})
set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(SDL2 DEFAULT_MSG SDL2_LIBRARY SDL2_INCLUDE_DIR)