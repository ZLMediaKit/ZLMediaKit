############################################################################
# FindX264.txt
# Copyright (C) 2015  Belledonne Communications, Grenoble France
#
############################################################################
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################
#
# - Find the x264 include file and library
#
#  X264_FOUND - system has x264
#  X264_INCLUDE_DIRS - the x264 include directory
#  X264_LIBRARIES - The libraries needed to use x264

include(CMakePushCheckState)
include(CheckCXXSymbolExists)

set(_X264_ROOT_PATHS
	${CMAKE_INSTALL_PREFIX}
)

find_path(X264_INCLUDE_DIRS
	NAMES x264.h
	HINTS _X264_ROOT_PATHS
	PATH_SUFFIXES include
)
if(X264_INCLUDE_DIRS)
	set(HAVE_X264_H 1)
endif()

find_library(X264_LIBRARIES
	NAMES x264
	HINTS _X264_ROOT_PATHS
	PATH_SUFFIXES bin lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(X264
	DEFAULT_MSG
	X264_INCLUDE_DIRS X264_LIBRARIES HAVE_X264_H
)

mark_as_advanced(X264_INCLUDE_DIRS X264_LIBRARIES HAVE_X264_H)