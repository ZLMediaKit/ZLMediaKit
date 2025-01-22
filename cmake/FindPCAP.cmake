# - Try to find libpcap
#
# Once done this will define
#  PCAP_FOUND        - System has libpcap
#  PCAP_INCLUDE_DIRS - The libpcap include directories
#  PCAP_LIBRARIES    - The libpcap library

# Find libpcap
FIND_PATH(
    PCAP_INCLUDE_DIRS
    NAMES pcap.h
)

FIND_LIBRARY(
    PCAP_LIBRARIES
    NAMES pcap
)

message(STATUS "PCAP LIBRARIES: " ${PCAP_LIBRARIES})
message(STATUS "PCAP INCLUDE DIRS: " ${PCAP_INCLUDE_DIRS})

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PCAP DEFAULT_MSG PCAP_LIBRARIES PCAP_INCLUDE_DIRS)