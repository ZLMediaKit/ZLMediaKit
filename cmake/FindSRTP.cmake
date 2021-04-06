############################################################################
# FindSRTP.txt
# Copyright (C) 2014  Belledonne Communications, Grenoble France
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
# - Find the SRTP include file and library
#
#  SRTP_FOUND - system has SRTP
#  SRTP_INCLUDE_DIRS - the SRTP include directory
#  SRTP_LIBRARIES - The libraries needed to use SRTP

set(_SRTP_ROOT_PATHS
        ${CMAKE_INSTALL_PREFIX}
        )

find_path(SRTP_INCLUDE_DIRS
        NAMES srtp2/srtp.h
        HINTS _SRTP_ROOT_PATHS  ${SRTP_PREFIX}
        PATH_SUFFIXES include
        )

if(SRTP_INCLUDE_DIRS)
    set(HAVE_SRTP_SRTP_H 1)
endif()

find_library(SRTP_LIBRARIES
        NAMES srtp2
        HINTS ${_SRTP_ROOT_PATHS} ${SRTP_PREFIX}
        PATH_SUFFIXES bin lib
        )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SRTP
        DEFAULT_MSG
        SRTP_INCLUDE_DIRS SRTP_LIBRARIES HAVE_SRTP_SRTP_H
        )

mark_as_advanced(SRTP_INCLUDE_DIRS SRTP_LIBRARIES HAVE_SRTP_SRTP_H)