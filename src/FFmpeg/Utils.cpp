/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */
#if defined(ENABLE_FFMPEG)
#if !defined(_WIN32)
#include <dlfcn.h>
#endif
#include "Common/config.h"
#include "Util/File.h"
#include "Util/uv_errno.h"
#include "Utils.h"
#include "mpeg-proto.h"
using namespace std;
using namespace toolkit;

namespace mediakit {
namespace ffmpeg {

string ffmpeg_err(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
    return errbuf;
}

std::shared_ptr<AVPacket> alloc_av_packet() {
    auto pkt = std::shared_ptr<AVPacket>(av_packet_alloc(), [](AVPacket *pkt) {
        // 减少引用计数，避免内存泄漏
        av_packet_unref(pkt);
        av_packet_free(&pkt);
    });
    pkt->data = nullptr; // packet data will be allocated by the encoder
    pkt->size = 0;
    return pkt;
}

//////////////////////////////////////////////////////////////////////////////////////////
static void on_ffmpeg_log(void *ctx, int level, const char *fmt, va_list args) {
    GET_CONFIG(bool, enable_ffmpeg_log, General::kEnableFFmpegLog);
    if (!enable_ffmpeg_log) {
        return;
    }
    LogLevel lev;
    switch (level) {
        case AV_LOG_FATAL: lev = LError; break;
        case AV_LOG_ERROR: lev = LError; break;
        case AV_LOG_WARNING: lev = LWarn; break;
        case AV_LOG_INFO: lev = LInfo; break;
        case AV_LOG_VERBOSE: lev = LDebug; break;
        case AV_LOG_DEBUG: lev = LDebug; break;
        case AV_LOG_TRACE: lev = LTrace; break;
        default: lev = LTrace; break;
    }
    LoggerWrapper::printLogV(::toolkit::getLogger(), lev, __FILE__, ctx ? av_default_item_name(ctx) : "NULL", level, fmt, args);
}

static bool setupFFmpeg_l() {
    av_log_set_level(AV_LOG_TRACE);
    av_log_set_flags(AV_LOG_PRINT_LEVEL);
    av_log_set_callback(on_ffmpeg_log);
#if (LIBAVCODEC_VERSION_MAJOR < 58)
    avcodec_register_all();
#endif
    return true;
}

void setupFFmpeg() {
    static auto flag = setupFFmpeg_l();
}

static bool checkIfSupportedNvidia_l() {
#if !defined(_WIN32)
    GET_CONFIG(bool, check_nvidia_dev, General::kCheckNvidiaDev);
    if (!check_nvidia_dev) {
        return false;
    }
    auto so = dlopen("libnvcuvid.so.1", RTLD_LAZY);
    if (!so) {
        WarnL << "libnvcuvid.so.1加载失败:" << get_uv_errmsg();
        return false;
    }
    dlclose(so);

    bool find_driver = false;
    File::scanDir(
        "/dev",
        [&](const string &path, bool is_dir) {
            if (!is_dir && start_with(path, "/dev/nvidia")) {
                // 找到nvidia的驱动
                find_driver = true;
                return false;
            }
            return true;
        },
        false);

    if (!find_driver) {
        WarnL << "英伟达硬件编解码器驱动文件 /dev/nvidia* 不存在";
    }
    return find_driver;
#else
    return false;
#endif
}

bool checkIfSupportedNvidia() {
    static auto ret = checkIfSupportedNvidia_l();
    return ret;
}
AVCodecID psi_to_avcodec_id(int psi_id) {
    switch (psi_id) {
        case PSI_STREAM_MPEG1: return AV_CODEC_ID_MPEG1VIDEO;
        case PSI_STREAM_MPEG2: return AV_CODEC_ID_MPEG2VIDEO;
        case PSI_STREAM_H264: return AV_CODEC_ID_H264;
        case PSI_STREAM_H265: return AV_CODEC_ID_HEVC;
        case PSI_STREAM_AAC: return AV_CODEC_ID_AAC;
        case PSI_STREAM_AUDIO_AC3: return AV_CODEC_ID_AC3;
        case PSI_STREAM_AUDIO_DTS: return AV_CODEC_ID_DTS;
        case PSI_STREAM_MP3: return AV_CODEC_ID_MP3;
        case PSI_STREAM_AUDIO_EAC3: return AV_CODEC_ID_EAC3;
        case PSI_STREAM_MPEG4_AAC_LATM: return AV_CODEC_ID_AAC_LATM;
        case PSI_STREAM_AUDIO_OPUS: return AV_CODEC_ID_OPUS;
        case PSI_STREAM_AUDIO_G711A: return AV_CODEC_ID_PCM_ALAW;
        case PSI_STREAM_AUDIO_G711U: return AV_CODEC_ID_PCM_MULAW;
        case PSI_STREAM_MPEG4: return AV_CODEC_ID_MPEG4;
        case PSI_STREAM_VIDEO_VC1: return AV_CODEC_ID_VC1;
        case PSI_STREAM_VIDEO_DIRAC: return AV_CODEC_ID_DIRAC;
        case PSI_STREAM_VIDEO_CAVS: return AV_CODEC_ID_CAVS;
        case PSI_STREAM_VP8: return AV_CODEC_ID_VP8;
        case PSI_STREAM_VP9: return AV_CODEC_ID_VP9;
        case PSI_STREAM_AV1: return AV_CODEC_ID_AV1;
        case PSI_STREAM_AUDIO_MPEG1: return AV_CODEC_ID_MP2;
        default: return (AVCodecID)psi_id;
    }
}

int avcodec_id_to_psi(int codec_id) {
    switch (codec_id) {
        case AV_CODEC_ID_MPEG1VIDEO: return PSI_STREAM_MPEG1;
        case AV_CODEC_ID_MPEG2VIDEO: return PSI_STREAM_MPEG2;
        case AV_CODEC_ID_H264: return PSI_STREAM_H264;
        case AV_CODEC_ID_HEVC: return PSI_STREAM_H265;
        case AV_CODEC_ID_AAC: return PSI_STREAM_AAC;
        case AV_CODEC_ID_AC3: return PSI_STREAM_AUDIO_AC3;
        case AV_CODEC_ID_DTS: return PSI_STREAM_AUDIO_DTS;
        case AV_CODEC_ID_MP3: return PSI_STREAM_MP3;
        case AV_CODEC_ID_EAC3: return PSI_STREAM_AUDIO_EAC3;
        case AV_CODEC_ID_AAC_LATM: return PSI_STREAM_MPEG4_AAC_LATM;
        case AV_CODEC_ID_PCM_ALAW: return PSI_STREAM_AUDIO_G711A;
        case AV_CODEC_ID_PCM_MULAW: return PSI_STREAM_AUDIO_G711U;
        case AV_CODEC_ID_OPUS: return PSI_STREAM_AUDIO_OPUS;
        case AV_CODEC_ID_MPEG4: return PSI_STREAM_MPEG4;
        case AV_CODEC_ID_VC1: return PSI_STREAM_VIDEO_VC1;
        case AV_CODEC_ID_DIRAC: return PSI_STREAM_VIDEO_DIRAC;
        case AV_CODEC_ID_CAVS: return PSI_STREAM_VIDEO_CAVS;
        case AV_CODEC_ID_VP8: return PSI_STREAM_VP8;
        case AV_CODEC_ID_VP9: return PSI_STREAM_VP9;
        case AV_CODEC_ID_AV1: return PSI_STREAM_AV1;
        default: return codec_id;
    }
}
bool is_audio_psi_codec(int psi_codec_id) {
    switch (psi_codec_id) {
        case PSI_STREAM_AAC:
        case PSI_STREAM_MPEG4_AAC:
        case PSI_STREAM_MPEG4_AAC_LATM:
        case PSI_STREAM_AUDIO_MPEG1:
        case PSI_STREAM_MP3:
        case PSI_STREAM_AUDIO_AC3:
        case PSI_STREAM_AUDIO_DTS:
        case PSI_STREAM_AUDIO_EAC3:
        case PSI_STREAM_AUDIO_SVAC:
        case PSI_STREAM_AUDIO_G711A:
        case PSI_STREAM_AUDIO_G711U:
        case PSI_STREAM_AUDIO_G722:
        case PSI_STREAM_AUDIO_G723:
        case PSI_STREAM_AUDIO_G729:
        case PSI_STREAM_AUDIO_OPUS: return true;
        default: return false;
    }
}
} // namespace ffmpeg
} // namespace mediakit
#endif // defined(ENABLE_FFMPEG)