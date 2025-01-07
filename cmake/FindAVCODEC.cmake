find_path(AVCODEC_INCLUDE_DIR
        NAMES libavcodec/avcodec.h
        HINTS ${FFMPEG_PATH_ROOT}
        PATH_SUFFIXES include)

find_library(AVCODEC_LIBRARY
        NAMES avcodec
        HINTS ${FFMPEG_PATH_ROOT}
        PATH_SUFFIXES bin lib)

set(AVCODEC_LIBRARIES ${AVCODEC_LIBRARY})
set(AVCODEC_INCLUDE_DIRS ${AVCODEC_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(AVCODEC DEFAULT_MSG AVCODEC_LIBRARY AVCODEC_INCLUDE_DIR)

# ����ҵ� libavcodec����ȡ�汾��
if(AVCODEC_FOUND)
    # ����汾�ű���
    set(AVCODEC_VERSION "")

    # ���� version_major.h �ļ������汾�ţ�
    if(EXISTS "${AVCODEC_INCLUDE_DIR}/libavcodec/version_major.h")
        file(STRINGS "${AVCODEC_INCLUDE_DIR}/libavcodec/version_major.h" AVCODEC_VERSION_MAJOR REGEX "^[ \t]*#define[ \t]+LIBAVCODEC_VERSION_MAJOR[ \t]+[0-9]+[ \t]*$")
        string(REGEX REPLACE "^[ \t]*#define[ \t]+LIBAVCODEC_VERSION_MAJOR[ \t]+([0-9]+)[ \t]*$" "\\1" AVCODEC_VERSION_MAJOR "${AVCODEC_VERSION_MAJOR}")
    endif()

    # ���� version.h �ļ����ΰ汾�ź�΢�汾�ţ�
    if(EXISTS "${AVCODEC_INCLUDE_DIR}/libavcodec/version.h")
        # ��ȡ�ΰ汾��
        file(STRINGS "${AVCODEC_INCLUDE_DIR}/libavcodec/version.h" AVCODEC_VERSION_MINOR REGEX "^[ \t]*#define[ \t]+LIBAVCODEC_VERSION_MINOR[ \t]+[0-9]+[ \t]*$")
        string(REGEX REPLACE "^[ \t]*#define[ \t]+LIBAVCODEC_VERSION_MINOR[ \t]+([0-9]+)[ \t]*$" "\\1" AVCODEC_VERSION_MINOR "${AVCODEC_VERSION_MINOR}")
        # ��ȡ΢�汾��
        file(STRINGS "${AVCODEC_INCLUDE_DIR}/libavcodec/version.h" AVCODEC_VERSION_MICRO REGEX "^[ \t]*#define[ \t]+LIBAVCODEC_VERSION_MICRO[ \t]+[0-9]+[ \t]*$")
        string(REGEX REPLACE "^[ \t]*#define[ \t]+LIBAVCODEC_VERSION_MICRO[ \t]+([0-9]+)[ \t]*$" "\\1" AVCODEC_VERSION_MICRO "${AVCODEC_VERSION_MICRO}")
    endif()

    # ƴ�Ӱ汾��
    if(AVCODEC_VERSION_MAJOR AND AVCODEC_VERSION_MINOR AND AVCODEC_VERSION_MICRO)
        set(AVCODEC_VERSION "${AVCODEC_VERSION_MAJOR}.${AVCODEC_VERSION_MINOR}.${AVCODEC_VERSION_MICRO}")
    endif()
endif()