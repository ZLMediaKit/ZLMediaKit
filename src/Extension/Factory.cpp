/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Factory.h"
#include "Rtmp/Rtmp.h"
#include "CommonRtmp.h"
#include "CommonRtp.h"
#include "Common/config.h"

using namespace std;
using namespace toolkit;

#define REGISTER_STATIC_VAR_INNER(var_name, line) var_name##_##line##__
#define REGISTER_STATIC_VAR(var_name, line) REGISTER_STATIC_VAR_INNER(var_name, line)

// 静态初始化阶段只登记插件地址，真正的注册动作延后到首次使用时执行  [AUTO-TRANSLATED:c9c1a0d2]
// Only record the plugin address at static-init time; the real registration is deferred to first use
// 注意: 必须在静态初始化路径上对插件符号取地址(odr-use)，否则 ext-codec 静态库中的obj不会被链接(LNK2019)
// Note: the plugin symbol must be odr-used from the static init path, otherwise the objects in the
// ext-codec static library won't be linked (LNK2019)
#define REGISTER_CODEC(plugin) \
extern CodecPlugin plugin;     \
static toolkit::onceToken REGISTER_STATIC_VAR(s_token, __LINE__) ([]() { \
    getPendingCodecPlugins().push_back(&plugin); \
});

namespace mediakit {

// 函数内静态对象，避免跨编译单元的静态初始化顺序问题  [AUTO-TRANSLATED:6f0a3e4b]
// Function-local static object to avoid static initialization order issues across translation units
static std::unordered_map<int, const CodecPlugin *> &getCodecPlugins() {
    static std::unordered_map<int, const CodecPlugin *> plugins;
    return plugins;
}

// 静态初始化阶段只登记插件地址，注册动作延后到首次使用时执行  [AUTO-TRANSLATED:c9c1a0d2]
// Only record plugin addresses at static-init time; the real registration is deferred to first use
static std::vector<const CodecPlugin *> &getPendingCodecPlugins() {
    static std::vector<const CodecPlugin *> plugins;
    return plugins;
}

// ext-codec 是独立静态库，插件符号的 odr-use 由 Factory.h 中的 REGISTER_CODEC 宏保证，
// 详见该宏注释: 依赖静态初始化路径上的引用，否则链接器不会加载插件所在的obj(LNK2019)
// ext-codec is a standalone static library; the odr-use of plugin symbols is guaranteed by the
// REGISTER_CODEC macro in Factory.h (see its comment): without a reference from the static init
// path, MSVC won't load the objects that define the plugins (LNK2019)
REGISTER_CODEC(vp8_plugin);
REGISTER_CODEC(vp9_plugin);
REGISTER_CODEC(h264_plugin);
REGISTER_CODEC(h265_plugin);
REGISTER_CODEC(av1_plugin);
REGISTER_CODEC(jpeg_plugin);
REGISTER_CODEC(aac_plugin);
REGISTER_CODEC(opus_plugin);
REGISTER_CODEC(g711a_plugin)
REGISTER_CODEC(g711u_plugin);
REGISTER_CODEC(l16_plugin);
REGISTER_CODEC(mp3_plugin);
REGISTER_CODEC(mp2v_plugin);
REGISTER_CODEC(mp2a_plugin);

// 首次调用时懒加载并注册所有编解码插件，之后直接查表，无需在各处重复调用  [AUTO-TRANSLATED:2a1f9d17]
// Lazily register all codec plugins on first call, then just look up the table
static const CodecPlugin *getCodecPlugin(CodecId codec) {
    static const std::unordered_map<int, const CodecPlugin *> &plugins = []() -> const std::unordered_map<int, const CodecPlugin *> & {
        for (auto plugin : getPendingCodecPlugins()) {
            Factory::registerPlugin(*plugin);
        }
        return getCodecPlugins();
    }();
    auto it = plugins.find((int)codec);
    return it == plugins.end() ? nullptr : it->second;
}

void Factory::registerPlugin(const CodecPlugin &plugin) {
    InfoL << "Load codec: " << getCodecName(plugin.getCodec());
    getCodecPlugins()[(int)(plugin.getCodec())] = &plugin;
}

Track::Ptr Factory::getTrackBySdp(const SdpTrack::Ptr &track) {
    auto codec = getCodecId(track->_codec);
    if (codec == CodecInvalid) {
        // 根据传统的payload type 获取编码类型以及采样率等信息  [AUTO-TRANSLATED:d01ca068]
        // Get the encoding type, sampling rate, and other information based on the traditional payload type
        codec = RtpPayload::getCodecId(track->_pt);
    }
    auto plugin = getCodecPlugin(codec);
    if (!plugin) {
        return getTrackByCodecId(codec, track->_samplerate, track->_channel);
    }
    return plugin->getTrackBySdp(track);
}

Track::Ptr Factory::getTrackByAbstractTrack(const Track::Ptr &track) {
    auto codec = track->getCodecId();
    if (track->getTrackType() == TrackVideo) {
        return getTrackByCodecId(codec);
    }
    auto audio_track = dynamic_pointer_cast<AudioTrack>(track);
    return getTrackByCodecId(codec, audio_track->getAudioSampleRate(), audio_track->getAudioChannel(), audio_track->getAudioSampleBit());
}

RtpCodec::Ptr Factory::getRtpEncoderByCodecId(CodecId codec, uint8_t pt) {
    auto plugin = getCodecPlugin(codec);
    if (!plugin) {
        WarnL << "Unsupported codec: " << getCodecName(codec) << ", use CommonRtpEncoder";
        return std::make_shared<CommonRtpEncoder>();
    }
    return plugin->getRtpEncoderByCodecId(pt);
}

RtpCodec::Ptr Factory::getRtpDecoderByCodecId(CodecId codec) {
    auto plugin = getCodecPlugin(codec);
    if (!plugin) {
        WarnL << "Unsupported codec: " << getCodecName(codec) << ", use CommonRtpDecoder";
        return std::make_shared<CommonRtpDecoder>(codec, 10 * 1024);
    }
    return plugin->getRtpDecoderByCodecId();
}

// ///////////////////////////rtmp相关///////////////////////////////////////////  [AUTO-TRANSLATED:da9645df]
// ///////////////////////////rtmp related///////////////////////////////////////////

static CodecId getVideoCodecIdByAmf(const AMFValue &val) {
    if (val.type() == AMF_STRING) {
        auto str = val.as_string();
        if (str == "avc1") {
            return CodecH264;
        }
        if (str == "hev1" || str == "hvc1") {
            return CodecH265;
        }
        WarnL << "Unsupported codec: " << str;
        return CodecInvalid;
    }

    if (val.type() != AMF_NULL) {
        auto type_id = (RtmpVideoCodec)val.as_integer();
        switch (type_id) {
            case RtmpVideoCodec::fourcc_avc1:
            case RtmpVideoCodec::h264: return CodecH264;
            case RtmpVideoCodec::fourcc_hevc:
            case RtmpVideoCodec::h265: return CodecH265;
            case RtmpVideoCodec::av1:
            case RtmpVideoCodec::fourcc_av1: return CodecAV1;
            case RtmpVideoCodec::vp8:
            case RtmpVideoCodec::fourcc_vp8: return CodecVP8;
            case RtmpVideoCodec::vp9:
            case RtmpVideoCodec::fourcc_vp9: return CodecVP9;
            default: WarnL << "Unsupported codec: " << (int)type_id; return CodecInvalid;
        }
    }
    return CodecInvalid;
}

Track::Ptr Factory::getTrackByCodecId(CodecId codec, int sample_rate, int channels, int sample_bit) {
    auto plugin = getCodecPlugin(codec);
    if (!plugin) {
        auto type = mediakit::getTrackType(codec);
        switch (type) {
            case TrackAudio: {
                WarnL << "Unsupported codec: " << getCodecName(codec) << ", use default audio track";
                return std::make_shared<AudioTrackImp>(codec, sample_rate, channels, sample_bit);
            }
            case TrackVideo: {
                WarnL << "Unsupported codec: " << getCodecName(codec) << ", use default video track";
                return std::make_shared<VideoTrackImp>(codec, 0, 0, 0);
            }
            default: WarnL << "Unsupported codec: " << getCodecName(codec); return nullptr;
        }
    }
    return plugin->getTrackByCodecId(sample_rate, channels, sample_bit);
}

Track::Ptr Factory::getVideoTrackByAmf(const AMFValue &amf) {
    CodecId codecId = getVideoCodecIdByAmf(amf);
    if (codecId == CodecInvalid) {
        return nullptr;
    }
    return getTrackByCodecId(codecId);
}

static CodecId getAudioCodecIdByAmf(const AMFValue &val) {
    if (val.type() == AMF_STRING) {
        auto str = val.as_string();
        if (str == "mp4a") {
            return CodecAAC;
        }
        WarnL << "Unsupported codec: " << str;
        return CodecInvalid;
    }

    if (val.type() != AMF_NULL) {
        auto type_id = (RtmpAudioCodec)val.as_integer();
        switch (type_id) {
            case RtmpAudioCodec::aac: return CodecAAC;
            case RtmpAudioCodec::mp3: return CodecMP3;
            case RtmpAudioCodec::adpcm: return CodecADPCM;
            case RtmpAudioCodec::g711a: return CodecG711A;
            case RtmpAudioCodec::g711u: return CodecG711U;
            case RtmpAudioCodec::opus:
            case RtmpAudioCodec::fourcc_opus: return CodecOpus;
            default: WarnL << "Unsupported codec: " << (int)type_id; return CodecInvalid;
        }
    }
    return CodecInvalid;
}

Track::Ptr Factory::getAudioTrackByAmf(const AMFValue &amf, int sample_rate, int channels, int sample_bit) {
    CodecId codecId = getAudioCodecIdByAmf(amf);
    if (codecId == CodecInvalid) {
        return nullptr;
    }
    return getTrackByCodecId(codecId, sample_rate, channels, sample_bit);
}

RtmpCodec::Ptr Factory::getRtmpDecoderByTrack(const Track::Ptr &track) {
    auto plugin = getCodecPlugin(track->getCodecId());
    if (!plugin) {
        WarnL << "Unsupported codec: " << track->getCodecName() << ", use CommonRtmpDecoder";
        return std::make_shared<CommonRtmpDecoder>(track);
    }
    return plugin->getRtmpDecoderByTrack(track);
}

RtmpCodec::Ptr Factory::getRtmpEncoderByTrack(const Track::Ptr &track) {
    auto plugin = getCodecPlugin(track->getCodecId());
    if (!plugin) {
        auto amf = Factory::getAmfByCodecId(track->getCodecId());
        WarnL << "Unsupported codec: " << track->getCodecName() << (amf ? ", use CommonRtmpEncoder" : "");
        return amf ? std::make_shared<CommonRtmpEncoder>(track) : nullptr;
    }
    return plugin->getRtmpEncoderByTrack(track);
}

AMFValue Factory::getAmfByCodecId(CodecId codecId) {
    GET_CONFIG(bool, enhanced, Rtmp::kEnhanced);
    switch (codecId) {
        case CodecAAC: return AMFValue((int)RtmpAudioCodec::aac);
        case CodecH264: return enhanced ? AMFValue((int)RtmpVideoCodec::fourcc_avc1) : AMFValue((int)RtmpVideoCodec::h264);
        case CodecH265: return enhanced ? AMFValue((int)RtmpVideoCodec::fourcc_hevc) : AMFValue((int)RtmpVideoCodec::h265);
        case CodecG711A: return AMFValue((int)RtmpAudioCodec::g711a);
        case CodecG711U: return AMFValue((int)RtmpAudioCodec::g711u);
        case CodecOpus: return enhanced ? AMFValue((int)RtmpAudioCodec::fourcc_opus) : AMFValue((int)RtmpAudioCodec::opus);
        case CodecADPCM: return AMFValue((int)RtmpAudioCodec::adpcm);
        case CodecMP3: return AMFValue((int)RtmpAudioCodec::mp3);
        case CodecAV1: return enhanced ? AMFValue((int)RtmpVideoCodec::fourcc_av1) : AMFValue((int)RtmpVideoCodec::av1);
        case CodecVP8: return enhanced ? AMFValue((int)RtmpVideoCodec::fourcc_vp8) : AMFValue((int)RtmpVideoCodec::vp8);
        case CodecVP9: return enhanced ? AMFValue((int)RtmpVideoCodec::fourcc_vp9) : AMFValue((int)RtmpVideoCodec::vp9);
        default: return AMFValue(AMF_NULL);
    }
}

Frame::Ptr Factory::getFrameFromPtr(CodecId codec, const char *data, size_t bytes, uint64_t dts, uint64_t pts) {
    auto plugin = getCodecPlugin(codec);
    if (!plugin) {
        // 创建不支持codec的frame  [AUTO-TRANSLATED:00936c6c]
        // Create a frame that does not support the codec
        return std::make_shared<FrameFromPtr>(codec, (char *)data, bytes, dts, pts);
    }
    return plugin->getFrameFromPtr(data, bytes, dts, pts);
}

Frame::Ptr Factory::getFrameFromBuffer(CodecId codec, Buffer::Ptr data, uint64_t dts, uint64_t pts) {
    auto frame = Factory::getFrameFromPtr(codec, data->data(), data->size(), dts, pts);
    if (!frame) {
        return nullptr;
    }
    return std::make_shared<FrameCacheAble>(frame, false, std::move(data));
}

} // namespace mediakit
