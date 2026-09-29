/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_DASHCODEC_H
#define ZLMEDIAKIT_DASHCODEC_H

#include <cstdint>
#include <cstdio>
#include <string>
#include "Extension/Track.h"

namespace mediakit {

/**
 * 由Track的extra data生成真实的RFC 6381 codecs字符串
 * dash.js用它做MediaSource.isTypeSupported()能力检查，必须给出真实值
 * Generate a real RFC 6381 codec string from the Track's extra data.
 * dash.js requires the codecs attribute on <Representation> to select decoders.
 *
 * H.264  → avc1.XXYYZZ        (from avcC: profile_idc, compat, level_idc)
 * H.265  → hvc1.X.Y.LZ.W      (from hvcC: profile_space+id, compat, tier+level, constraints)
 * AAC    → mp4a.40.N          (from AudioSpecificConfig: audio object type)
 * VP8/9  → vp8 / vp9
 * AV1    → av01.P.LLT.C       (from AV1CodecConfigurationRecord)
 * Opus   → opus
 * G.711  → pcma / pcmu
 * MP3    → mp4a.6b
 */
inline std::string getDashCodecString(const Track::Ptr &track) {
    auto id = track->getCodecId();
    auto extra = track->getExtraData();
    const uint8_t *data = extra ? (const uint8_t *)extra->data() : nullptr;
    size_t size = extra ? extra->size() : 0;
    char buf[64];

    switch (id) {
        case CodecH264: {
            // avcC body: version(1) profile_idc(1) compat(1) level_idc(1) ...
            if (data && size >= 4) {
                snprintf(buf, sizeof(buf), "avc1.%02x%02x%02x", data[1], data[2], data[3]);
                return buf;
            }
            return "avc1.4d001f"; // fallback: Main Profile Level 3.1
        }

        case CodecH265: {
            // hvcC body per ISO 14496-15 HEVCDecoderConfigurationRecord:
            //   byte 0 = configurationVersion
            //   byte 1 = [profile_space(2) | tier_flag(1) | profile_idc(5)]
            //   bytes 2-5 = profile_compatibility_flags
            //   bytes 6-11 = constraint_indicator (48-bit)
            //   byte 12 = general_level_idc
            if (data && size >= 13) {
                uint8_t ps = (data[1] >> 6) & 0x03; // general_profile_space
                uint8_t pid = data[1] & 0x1f; // general_profile_idc
                uint32_t pcf = ((uint32_t)data[2] << 24) | ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 8) | (uint32_t)data[5];
                char tier = ((data[1] >> 5) & 0x01) ? 'H' : 'L';
                uint8_t level = data[12]; // general_level_idc
                uint64_t cons = 0; // constraint_indicator (48-bit)
                for (int k = 6; k < 12; ++k) {
                    cons = (cons << 8) | data[k];
                }
                // RFC 6381 HEVC codec string:
                //   hvc1.<profile_space><profile_idc>.<compat_hex>.<tier><level>.<constraint_hex>
                // The compat/constraint fields are hex with trailing zeros trimmed
                // (0x60000000 -> "6") and the level is decimal (L150, not L96),
                // otherwise Chrome's MediaSource.isTypeSupported() rejects it.
                auto hextrim = [](uint64_t v) -> std::string {
                    if (v == 0) {
                        return "0";
                    }
                    char b[24];
                    snprintf(b, sizeof(b), "%llX", (unsigned long long)v);
                    std::string s = b;
                    while (s.size() > 1 && s.back() == '0') {
                        s.pop_back();
                    }
                    return s;
                };
                std::string cs = "hvc1.";
                if (ps) {
                    cs += static_cast<char>('A' + ps - 1);
                }
                cs += std::to_string((unsigned)pid);
                cs += '.' + hextrim(pcf);
                cs += '.';
                cs += tier;
                cs += std::to_string((unsigned)level);
                cs += '.' + hextrim(cons);
                return cs;
            }
            return "hvc1.1.6.L120.B0"; // fallback: Main Profile
        }

        case CodecAAC: {
            // AudioSpecificConfig: 2 bytes
            //   bits 0-4:  audioObjectType (5 bits for values 1-31)
            //   bits 5-8:  samplingFrequencyIndex
            //   bits 9-12: channelConfiguration
            if (data && size >= 2) {
                uint8_t aot = (data[0] >> 3) & 0x1f;
                snprintf(buf, sizeof(buf), "mp4a.40.%u", aot);
                return buf;
            }
            return "mp4a.40.2"; // fallback: AAC-LC
        }

        case CodecVP8: return "vp8";
        case CodecVP9: return "vp9";

        case CodecAV1: {
            // AV1CodecConfigurationRecord:
            //   byte 0: marker(1) | version(7) = 0x81
            //   byte 1: seq_profile(3) | seq_level_idx_0(5)
            //   byte 2: seq_tier_0(1) | high_bitdepth(1) | twelve_bit(1) |
            //           monochrome(1) | chroma_subsampling_x(1) |
            //           chroma_subsampling_y(1) | chroma_sample_position(2)
            if (data && size >= 3) {
                uint8_t profile = (data[1] >> 5) & 0x07;
                uint8_t level = data[1] & 0x1f;
                uint8_t tier = (data[2] >> 7) & 0x01;
                uint8_t bitdepth = 8 + 2 * ((data[2] >> 6) & 0x01) + 2 * ((data[2] >> 5) & 0x01);
                snprintf(buf, sizeof(buf), "av01.%u.%02u%c.%02u", profile, level, tier ? 'H' : 'M', bitdepth);
                return buf;
            }
            return "av01.0.01M.08";
        }

        case CodecOpus: return "opus";
        case CodecMP3: return "mp4a.6b";

        case CodecG711A: return "pcma";
        case CodecG711U: return "pcmu";

        default: return "";
    }
}

} // namespace mediakit
#endif // ZLMEDIAKIT_DASHCODEC_H
