/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "Extension/Factory.h"
#include "Common/config.h"
#include "Rtmp/RtmpDemuxer.h"
#include "ext-codec/AV1.h"
#include "ext-codec/VpxRtmp.h"

using namespace mediakit;

namespace {
int failures = 0;
void require(bool condition, const std::string &message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << std::endl;
    }
}

std::string unhex(const std::string &hex) {
    std::string bytes;
    for (size_t i = 0; i < hex.size(); i += 2) {
        bytes.push_back(static_cast<char>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    return bytes;
}

// Generated with FFmpeg/libaom: color=c=blue:size=2560x1440:rate=60,
// -frames:v 4 -cpu-used 8 -lag-in-frames 0 -g 2 -f ivf.
// Keep a real temporal delimiter + sequence header + keyframe, not a ZLM encoder round trip.
const auto keyframe = unhex("12000a0c00000062ea7fecfb95f200803233100085a000008000200c58f6de6dde4989ff832ffe1dbcaea584f44d6e528c552631c8ae61c48bc4561172a7963fac64cd2d7d");
const auto interframe = unhex("12003224300380800000468a20000090000800fbc4ef4f40dc1b8975fc537624dc4d9213db48b1c0");
const auto av1c = unhex("810c0c000a0c00000062ea7fecfb95f20080");

RtmpPacket::Ptr packet(const std::string &body, uint32_t dts = 0, uint8_t type = MSG_VIDEO) {
    auto pkt = RtmpPacket::create();
    pkt->buffer.assign(body.data(), body.size());
    pkt->body_size = body.size();
    pkt->type_id = type;
    pkt->time_stamp = dts;
    pkt->chunk_id = type == MSG_VIDEO ? CHUNK_VIDEO : CHUNK_AUDIO;
    pkt->stream_index = STREAM_MEDIA;
    return pkt;
}
RtmpPacket::Ptr enhanced(uint8_t type, const std::string &body, uint32_t dts = 0,
                         const std::string &fourcc = "av01") {
    return packet(std::string(1, static_cast<char>(0x90 | type)) + fourcc + body, dts);
}

void testCodedFrames() {
    auto track = std::make_shared<AV1Track>();
    VpxRtmpDecoder decoder(track);
    size_t frames = 0;
    track->addDelegate([&](const Frame::Ptr &frame) {
        ++frames;
        std::cout << "decoded frame size=" << frame->size() << " dts=" << frame->dts()
                  << " pts=" << frame->pts() << " dimensions=" << track->getVideoWidth()
                  << 'x' << track->getVideoHeight() << " fps=" << track->getVideoFps() << '\n';
        const auto &expected = frames == 1 ? keyframe : interframe;
        require(std::string(frame->data(), frame->size()) == expected, "AV1 CodedFrames preserves every OBU byte");
        require(frame->dts() == frame->pts(), "AV1 has no composition-time field");
        require(frame->dts() == (frames == 1 ? 100U : 117U), "AV1 DTS is preserved");
        return true;
    });
    decoder.inputRtmp(enhanced(0, av1c));
    decoder.inputRtmp(enhanced(1, keyframe, 100));
    decoder.inputRtmp(enhanced(1, interframe, 117));
    require(frames == 2, "both AV1 temporal units are delivered");
    require(track->getVideoWidth() == 2560 && track->getVideoHeight() == 1440,
            "AV1 CodedFrames produces 2560x1440 metadata");

    // Older ZLM senders used CodedFramesX with no CTS. Continue accepting that layout.
    auto compatibility = std::make_shared<AV1Track>();
    VpxRtmpDecoder old_decoder(compatibility);
    old_decoder.inputRtmp(enhanced(3, keyframe, 100));
    require(compatibility->getVideoWidth() == 2560 && compatibility->getVideoHeight() == 1440,
            "legacy ZLM CodedFramesX remains readable");
}

void testWithoutTemporalDelimiter() {
    // A temporal delimiter is optional in the low-overhead bitstream. Removing
    // it must not damage the sequence header.
    auto track = std::make_shared<AV1Track>();
    VpxRtmpDecoder decoder(track);
    decoder.inputRtmp(enhanced(0, av1c));
    decoder.inputRtmp(enhanced(1, keyframe.substr(2), 100));
    require(track->ready() && track->getVideoWidth() == 2560 && track->getVideoHeight() == 1440,
            "AV1 without a temporal delimiter is a ready video track");
}

void testConfiguration() {
    aom_av1_t context {};
    auto loaded = aom_av1_codec_configuration_record_load(
        reinterpret_cast<const uint8_t *>(av1c.data()), av1c.size(), &context);
    std::cout << "av1C load=" << loaded << " dimensions=" << context.width << 'x' << context.height;
    auto parsed = aom_av1_codec_configuration_record_init(&context, keyframe.data(), keyframe.size());
    std::cout << "; intact frame init=" << parsed << " dimensions=" << context.width << 'x' << context.height << '\n';
    require(loaded == static_cast<int>(av1c.size()) && parsed == 0
            && context.width == 2560 && context.height == 1440, "pinned parser accepts the intact fixture");

    AV1Track track;
    track.setExtraData(reinterpret_cast<const uint8_t *>(av1c.data()), av1c.size());
    require(track.ready() && track.getVideoWidth() == 2560 && track.getVideoHeight() == 1440,
            "SequenceStart parses configOBUs before any coded frame");
    auto extra = track.getExtraData();
    require(extra && std::string(extra->data(), extra->size()) == av1c, "av1C round trip preserves configuration");
    for (unsigned i = 0; i < 200; ++i) {
        track.inputFrame(Factory::getFrameFromPtr(CodecAV1, keyframe.data(), keyframe.size(), i * 17, i * 17));
    }
    extra = track.getExtraData();
    require(extra && extra->size() == av1c.size(), "repeated sequence headers do not accumulate in av1C");
    track.setExtraData(reinterpret_cast<const uint8_t *>("\x81"), 1);
    require(track.getVideoWidth() == 2560 && track.getVideoHeight() == 1440,
            "truncated av1C does not destroy known dimensions");
    // Failed frame parsing must not overwrite the last good configuration.
    const auto truncated = unhex("0a0c0000");
    track.inputFrame(Factory::getFrameFromPtr(CodecAV1, truncated.data(), truncated.size(), 4000, 4000));
    track.inputFrame(Factory::getFrameFromPtr(CodecAV1, interframe.data(), interframe.size(), 4017, 4017));
    require(track.getVideoWidth() == 2560 && track.getVideoHeight() == 1440,
            "bad or sequence-header-free frames retain known dimensions");
    auto clone = std::dynamic_pointer_cast<VideoTrack>(track.clone());
    require(clone->ready() && clone->getVideoWidth() == 2560, "track clones retain configuration");
}

void testDiscovery() {
    RtmpDemuxer demuxer;
    // Unknown FourCC followed by a usable AV1 SequenceStart. No onMetaData rescue.
    demuxer.inputRtmp(enhanced(0, av1c, 0, "????"));
    require(demuxer.getTracks(false).empty(), "unknown codec does not invent a track");
    demuxer.inputRtmp(enhanced(0, av1c));
    demuxer.inputRtmp(enhanced(1, keyframe));
    auto tracks = demuxer.getTracks(false);
    require(tracks.size() == 1 && tracks[0]->getCodecId() == CodecAV1,
            "video discovery retries after an unsupported first packet");
}

// Inspect output at the wire boundary, not through a decoder sharing the same bug.
class CapturingVpxEncoder : public VpxRtmpEncoder {
public:
    explicit CapturingVpxEncoder(const Track::Ptr &track) : VpxRtmpEncoder(track) {
        auto ring = std::make_shared<RingType>();
        ring->setDelegate(std::make_shared<Capture>(output));
        setRtmpRing(ring);
    }
    RtmpPacket::Ptr output;
private:
    class Capture : public toolkit::RingDelegate<RtmpPacket::Ptr> {
    public:
        explicit Capture(RtmpPacket::Ptr &output) : _output(output) {}
        void onWrite(RtmpPacket::Ptr packet, bool) override { _output = packet; }
    private:
        RtmpPacket::Ptr &_output;
    };
};

void testVpxPacketLayouts() {
    for (auto codec : {CodecAV1, CodecVP8, CodecVP9}) {
        const std::string payload = codec == CodecAV1 ? unhex("1a0180") : unhex("010203040506");
        const std::string fourcc = codec == CodecAV1 ? "av01" : codec == CodecVP8 ? "vp08" : "vp09";
        auto track = std::make_shared<VideoTrackImp>(codec, 2560, 1440, 60);
        VpxRtmpDecoder decoder(track);
        size_t frames = 0;
        track->addDelegate([&](const Frame::Ptr &frame) {
            ++frames;
            require(std::string(frame->data(), frame->size()) == payload, "short VPx CodedFrames remains intact");
            require(frame->dts() == 100 && frame->pts() == 100, "VPx CodedFrames uses the RTMP timestamp");
            return true;
        });
        decoder.inputRtmp(enhanced(1, payload, 100, fourcc));
        require(frames == 1, "short VPx payload is delivered");

        toolkit::mINI::Instance()[Rtmp::kEnhanced] = true;
        CapturingVpxEncoder encoder(track);
        // Even if the caller supplies different PTS, enhanced VPx has no CTS field.
        auto frame = Factory::getFrameFromPtr(codec, payload.data(), payload.size(), 100, 117);
        encoder.inputFrame(frame);
        require(encoder.output != nullptr, "VPx encoder emits a packet");
        if (encoder.output) {
            require(encoder.output->time_stamp == 117, "enhanced VPx carries presentation time without CTS");
            const auto expected = std::string(1, static_cast<char>(0xa1)) + fourcc + payload;
            require(std::string(encoder.output->data(), encoder.output->size()) == expected,
                    "VPx encoder emits CodedFrames with no CTS bytes");
        }

        // Classic domestic-extension packets still have their SI24 offset.
        VpxRtmpDecoder classic_decoder(track);
        track->clear();
        bool classic_frame = false;
        track->addDelegate([&](const Frame::Ptr &decoded) {
            classic_frame = true;
            require(decoded->dts() == 100 && decoded->pts() == 117, "classic VPx preserves composition time");
            require(std::string(decoded->data(), decoded->size()) == payload, "classic VPx preserves payload");
            return true;
        });
        auto body = std::string(1, static_cast<char>(0x20 | getCodecFlags(codec))) + unhex("01000011") + payload;
        classic_decoder.inputRtmp(packet(body, 100));
        require(classic_frame, "classic VPx still decodes");
    }
}

void testMissingOrInvalidFps() {
    const std::vector<AMFValue> invalid_rates = {
        AMFValue(), AMFValue("60"), AMFValue(0.0), AMFValue(-1.0),
        AMFValue(std::numeric_limits<double>::infinity()),
        AMFValue(std::numeric_limits<double>::quiet_NaN())
    };
    for (const auto &fps : invalid_rates) {
        RtmpDemuxer demuxer;
        AMFValue metadata(AMF_OBJECT);
        metadata.set("videocodecid", static_cast<int>(RtmpVideoCodec::fourcc_av1));
        metadata.set("framerate", fps);
        demuxer.loadMetaData(metadata);
        auto tracks = demuxer.getTracks(false);
        require(tracks.size() == 1, "invalid FPS does not prevent track discovery");
        if (!tracks.empty()) {
            auto video = std::dynamic_pointer_cast<VideoTrack>(tracks[0]);
            require(video->getVideoFps() == 0, "invalid/unknown AV1 FPS is not fabricated as 30");
        }
    }
}

void testMetadataFps() {
    for (double fps : {60.0, 60000.0 / 1001.0}) {
        RtmpDemuxer demuxer;
        AMFValue metadata(AMF_OBJECT);
        metadata.set("videocodecid", static_cast<int>(RtmpVideoCodec::fourcc_av1));
        metadata.set("framerate", fps);
        demuxer.loadMetaData(metadata);
        demuxer.inputRtmp(enhanced(0, av1c));
        demuxer.inputRtmp(enhanced(1, keyframe));
        auto tracks = demuxer.getTracks(false);
        require(tracks.size() == 1, "onMetaData creates AV1 track");
        if (!tracks.empty()) {
            auto video = std::dynamic_pointer_cast<VideoTrack>(tracks[0]);
            std::cout << "onMetaData fps=" << fps << " track fps=" << video->getVideoFps() << '\n';
            require(std::abs(video->getVideoFps() - fps) < 0.001, "AV1 honors fractional onMetaData framerate");
            auto clone = std::dynamic_pointer_cast<VideoTrack>(video->clone());
            require(std::abs(clone->getVideoFps() - fps) < 0.001, "AV1 FPS survives cloning into the media sink");
        }
    }
}
} // namespace

int main() {
    testDiscovery();
    testConfiguration();
    testWithoutTemporalDelimiter();
    testCodedFrames();
    testMetadataFps();
    testMissingOrInvalidFps();
    testVpxPacketLayouts();
    std::cout << failures << " failed checks\n";
    return failures ? 1 : 0;
}
