/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_UTILS_H
#define ZLMEDIAKIT_UTILS_H
#if defined(ENABLE_FFMPEG)
#ifdef __cplusplus
#include "Network/Buffer.h"
#include "Util/TimeTicker.h"
#include "Util/logger.h"
extern "C" {
#endif
#include "libavcodec/avcodec.h"
#include "libavcodec/bsf.h"
#include "libavutil/intreadwrite.h"
#include "libswresample/swresample.h"
#include <libavutil/audio_fifo.h>
#include <libavutil/fifo.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#ifdef __cplusplus
}
#endif
namespace mediakit {
namespace ffmpeg {
//todo 未来集成FFmpeg的相关工具函数
class Utils {
    Utils() = default;
    ~Utils() = default;
};
void setupFFmpeg();
std::string ffmpeg_err(int err_num);
bool checkIfSupportedNvidia();
std::shared_ptr<AVPacket> alloc_av_packet();
AVCodecID psi_to_avcodec_id(int psi_id);
int avcodec_id_to_psi(int codec_id);
bool is_audio_psi_codec(int psi_codec_id);
} // namespace ffmpeg
} // namespace mediakit
#endif // defined(ENABLE_FFMPEG)
#endif // ZLMEDIAKIT_UTILS_H
