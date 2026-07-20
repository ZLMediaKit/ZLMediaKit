/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "H264.h"
#include "H264Rtmp.h"
#include "H264Rtp.h"
#include "Util/logger.h"
#include "Util/base64.h"
#include "Common/Parser.h"
#include "Common/config.h"
#include "Extension/Factory.h"

#ifdef ENABLE_MP4
#include "mpeg4-avc.h"
#endif

#include <vector>
#include <stdexcept>
#include <climits>
#include <limits>

using namespace std;
using namespace toolkit;

namespace mediakit {

// ---- 内部比特流工具 ----
namespace {

// SPS 的标准语法规模远小于 1 MiB；在复制并去除防竞争字节前设置宽松硬上限，避免单个恶意 NALU 触发同等规模的第二次分配。
// Standard SPS syntax is far smaller than 1 MiB; a generous pre-copy cap prevents one hostile NALU from forcing a second allocation of the same scale.
static constexpr size_t kMaxParameterSetSize = 1024 * 1024;

// 去除 RBSP 防竞争字节 (0x00 0x00 0x03 -> 0x00 0x00)
static std::vector<uint8_t> rbsp_from_nalu(const uint8_t *data, size_t size) {
    std::vector<uint8_t> out;
    out.reserve(size);
    for (size_t i = 0; i < size; ) {
        if (i + 2 < size && data[i] == 0x00 && data[i+1] == 0x00 && data[i+2] == 0x03) {
            out.push_back(0x00);
            out.push_back(0x00);
            i += 3;
        } else {
            out.push_back(data[i++]);
        }
    }
    return out;
}

struct BitStream {
    const uint8_t *buf;
    size_t size; // bytes
    size_t pos;  // bit position

    BitStream(const uint8_t *b, size_t s) : buf(b), size(s), pos(0) {}

    bool eof() const { return pos >= size * 8; }

    size_t bits_left() const { return size * 8 - pos; }

    uint32_t read_bits(int n) {
        if (n < 0 || n > 32 || (size_t)n > bits_left()) {
            throw std::runtime_error("eof");
        }
        uint32_t val = 0;
        for (int i = 0; i < n; i++) {
            val = (val << 1) | ((buf[pos / 8] >> (7 - pos % 8)) & 1);
            pos++;
        }
        return val;
    }

    void skip_bits(int n) {
        if (n < 0 || (size_t)n > bits_left()) {
            throw std::runtime_error("eof");
        }
        pos += n;
    }

    uint32_t read_ue() { // Exp-Golomb unsigned
        int zeros = 0;
        // Exp-Golomb 编码必须包含值为 1 的停止位；此前在停止位前遇到 EOF 会被误判为数值 0，并可能驱动后续循环空转。
        // Exp-Golomb codes require a one-bit terminator; treating EOF before it as zero could feed bogus counts into later loops.
        while (true) {
            if (eof()) {
                throw std::runtime_error("eof before exp-golomb stop bit");
            }
            if (read_bits(1) != 0) {
                break;
            }
            // 本读取器返回 uint32_t，最多只能接受 31 个前导零；32 个前导零需要 33 位编码，且会让后续 se(v) 映射越界。
            // This uint32_t reader accepts at most 31 leading zeroes; 32 require a 33-bit code and would overflow later se(v) mapping.
            if (++zeros >= 32) {
                throw std::runtime_error("exp-golomb overflow");
            }
        }
        if (zeros == 0) return 0;
        return (1u << zeros) - 1 + read_bits(zeros);
    }

    int32_t read_se() { // Exp-Golomb signed
        uint32_t v = read_ue();
        return (v & 1) ? (int32_t)((v + 1) >> 1) : -(int32_t)(v >> 1);
    }

};

} // anonymous namespace

// ---- H264 SPS 解析 ----
static bool getAVCInfo(const char *sps_raw, size_t sps_len, int &iVideoWidth, int &iVideoHeight, float &iVideoFps) {
    if (sps_len < 4 || sps_len > kMaxParameterSetSize) return false;
    try {
        // RBSP 分配也可能因恶意超大 NALU 抛出异常；将其纳入保护范围，才能维持本接口只返回 false 的失败语义。
        // RBSP allocation can throw for a maliciously large NALU; keep it inside the guard so this boolean API fails with false.
        // sps_raw[0] 是 NAL header，从第 1 字节开始是 RBSP
        auto rbsp = rbsp_from_nalu((const uint8_t *)sps_raw + 1, sps_len - 1);
        if (rbsp.size() < 3) return false;

        BitStream bs(rbsp.data(), rbsp.size());
        int parsed_width = 0;
        int parsed_height = 0;
        float parsed_fps = 0.0f;

        uint8_t profile_idc = (uint8_t)bs.read_bits(8); // profile_idc
        bs.skip_bits(8); // constraint flags + reserved
        bs.skip_bits(8); // level_idc
        uint32_t seq_parameter_set_id = bs.read_ue();
        // H.264 只定义 0..31 的 SPS id；拒绝相邻越界值，避免接受无法由标准参数集表表示的配置。
        // H.264 defines SPS ids only in 0..31; reject the adjacent out-of-range value instead of accepting an unrepresentable parameter set.
        if (seq_parameter_set_id > 31) {
            return false;
        }

        uint32_t chroma_format_idc = 1;
        if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 ||
            profile_idc == 244 || profile_idc == 44  || profile_idc == 83  ||
            profile_idc == 86  || profile_idc == 118 || profile_idc == 128) {
            chroma_format_idc = bs.read_ue();
            if (chroma_format_idc > 3) {
                return false;
            }
            if (chroma_format_idc == 3) bs.skip_bits(1); // separate_colour_plane_flag
            uint32_t bit_depth_luma_minus8 = bs.read_ue();
            uint32_t bit_depth_chroma_minus8 = bs.read_ue();
            // 两个位深字段分别受 0..6 限制，但语法允许它们取不同值；强制相等会拒绝可以安全提取尺寸的合法 SPS。
            // Each bit-depth offset is independently limited to 0..6, while the syntax permits different values; requiring equality rejects valid SPS whose dimensions are still safe to extract.
            if (bit_depth_luma_minus8 > 6 || bit_depth_chroma_minus8 > 6) {
                return false;
            }
            bs.skip_bits(1); // qpprime_y_zero_transform_bypass_flag
            if (bs.read_bits(1)) { // seq_scaling_matrix_present_flag
                int cnt = (chroma_format_idc != 3) ? 8 : 12;
                for (int i = 0; i < cnt; i++) {
                    if (bs.read_bits(1)) { // seq_scaling_list_present_flag
                        int sz = (i < 6) ? 16 : 64;
                        int last = 8, next = 8;
                        for (int j = 0; j < sz; j++) {
                            if (next != 0) {
                                // delta_scale 来自不可信位流，直接用 int 相加可能触发有符号溢出；宽类型和规范化取模可保持标准语义。
                                // delta_scale is untrusted and may overflow int addition; wide arithmetic plus normalized modulo preserves the SPS rule.
                                int64_t value = (int64_t)last + bs.read_se() + 256;
                                next = (int)(value % 256);
                                if (next < 0) {
                                    next += 256;
                                }
                            }
                            last = (next == 0) ? last : next;
                        }
                    }
                }
            }
        }

        uint32_t log2_max_frame_num_minus4 = bs.read_ue();
        // 规范范围为 0..12；即使当前只提取宽高，也不能把越界 SPS 当作有效配置发布。
        // The specified range is 0..12; even a dimensions-only parser must not publish an out-of-range SPS as valid configuration.
        if (log2_max_frame_num_minus4 > 12) {
            return false;
        }
        uint32_t pic_order_cnt_type = bs.read_ue();
        if (pic_order_cnt_type == 0) {
            uint32_t log2_max_pic_order_cnt_lsb_minus4 = bs.read_ue();
            // POC LSB 位数增量同样仅允许 0..12，越界值会描述标准外的帧序号空间。
            // The POC-LSB bit-count offset is likewise limited to 0..12; larger values describe a non-standard picture-order space.
            if (log2_max_pic_order_cnt_lsb_minus4 > 12) {
                return false;
            }
        } else if (pic_order_cnt_type == 1) {
            bs.skip_bits(1); // delta_pic_order_always_zero_flag
            bs.read_se();    // offset_for_non_ref_pic
            bs.read_se();    // offset_for_top_to_bottom_field
            uint32_t n = bs.read_ue();
            // 标准只允许最多 255 个 offset；先校验再循环，避免恶意计数长时间占用媒体输入线程。
            // The standard allows at most 255 offsets; validate before looping so a hostile count cannot monopolize the media input thread.
            if (n > 255) {
                return false;
            }
            for (uint32_t i = 0; i < n; i++) bs.read_se();
        } else if (pic_order_cnt_type != 2) {
            // 仅 0、1、2 是有效 POC 类型；继续解析未知类型会使后续字段错位并可能接受伪造尺寸。
            // Only POC types 0, 1, and 2 are valid; continuing with another value misaligns later fields and may accept forged dimensions.
            return false;
        }
        uint32_t max_num_ref_frames = bs.read_ue();
        // H.264 解码图像缓冲区最多表示 16 个参考帧；提前拒绝 17 可保持与原解析器的标准边界一致。
        // The H.264 decoded-picture buffer represents at most 16 reference frames; rejecting 17 preserves the prior parser's standard boundary.
        if (max_num_ref_frames > 16) {
            return false;
        }
        bs.skip_bits(1); // gaps_in_frame_num_value_allowed_flag

        uint32_t pic_width_in_mbs_minus1       = bs.read_ue();
        uint32_t pic_height_in_map_units_minus1 = bs.read_ue();
        uint32_t frame_mbs_only_flag            = bs.read_bits(1);
        if (!frame_mbs_only_flag) bs.skip_bits(1); // mb_adaptive_frame_field_flag
        bs.skip_bits(1); // direct_8x8_inference_flag

        uint32_t crop_left = 0, crop_right = 0, crop_top = 0, crop_bottom = 0;
        if (bs.read_bits(1)) { // frame_cropping_flag
            crop_left   = bs.read_ue();
            crop_right  = bs.read_ue();
            crop_top    = bs.read_ue();
            crop_bottom = bs.read_ue();
        }

        uint32_t crop_unit_x = 1;
        uint32_t crop_unit_y = 2 - frame_mbs_only_flag;
        if (chroma_format_idc == 1) {
            crop_unit_x = 2;
            crop_unit_y = 2 * (2 - frame_mbs_only_flag);
        } else if (chroma_format_idc == 2) {
            crop_unit_x = 2;
            crop_unit_y = 2 - frame_mbs_only_flag;
        }

        // 宏块计数来自不可信 ue(v)，必须在加一前提升到 64 位；否则未来若放宽读取上限，uint32_t 加法可能先回绕为零。
        // Macroblock counts are untrusted ue(v) values and must be widened before adding one; otherwise a future reader extension could wrap uint32_t to zero first.
        uint64_t raw_width  = ((uint64_t)pic_width_in_mbs_minus1 + 1) * 16;
        uint64_t raw_height = ((uint64_t)pic_height_in_map_units_minus1 + 1) * 16 * (2 - frame_mbs_only_flag);
        uint64_t crop_w = ((uint64_t)crop_left + crop_right) * crop_unit_x;
        uint64_t crop_h = ((uint64_t)crop_top  + crop_bottom) * crop_unit_y;
        if (crop_w >= raw_width || crop_h >= raw_height) return false;
        uint64_t display_width = raw_width - crop_w;
        uint64_t display_height = raw_height - crop_h;
        // 输出接口使用 int；转换前限制范围，避免恶意尺寸触发实现定义的窄化并污染下游元数据。
        // The output API uses int; range-check before narrowing so hostile dimensions cannot produce implementation-defined metadata.
        if (display_width > INT_MAX || display_height > INT_MAX) {
            return false;
        }
        parsed_width = (int)display_width;
        parsed_height = (int)display_height;

        if (bs.read_bits(1)) { // vui_parameters_present_flag
            if (bs.read_bits(1)) { // aspect_ratio_info_present_flag
                uint32_t ar = bs.read_bits(8);
                if (ar == 255) bs.skip_bits(32); // sar width+height
            }
            if (bs.read_bits(1)) bs.skip_bits(1); // overscan
            if (bs.read_bits(1)) {                // video_signal_type_present_flag
                bs.skip_bits(3 + 1); // video_format + video_full_range_flag
                if (bs.read_bits(1)) bs.skip_bits(24); // colour_description_present_flag
            }
            if (bs.read_bits(1)) { // chroma_loc_info_present_flag
                bs.read_ue(); bs.read_ue();
            }
            if (bs.read_bits(1)) { // timing_info_present_flag
                uint32_t num_units_in_tick = bs.read_bits(32);
                uint32_t time_scale        = bs.read_bits(32);
                bs.skip_bits(1); // fixed_frame_rate_flag
                if (num_units_in_tick > 0) {
                    parsed_fps = (float)time_scale / (2.0f * (float)num_units_in_tick);
                }
            }
        }
        if (parsed_width <= 0 || parsed_height <= 0) {
            return false;
        }
        // 本接口只提取宽高和 VUI 时序；HRD 及其后的尾部语法不影响这些结果，也不应把元数据提取器扩展成完整合规验证器。
        // This API only extracts dimensions and VUI timing; HRD and later tail syntax do not affect them and must not turn this metadata reader into a full conformance validator.
        // 已消费字段全部成功后再写回，既保留项目原有的宽松尾部兼容性，也避免异常发布部分结果。
        // Commit only after every consumed field succeeds, preserving the project's permissive tail compatibility without publishing partial results on failure.
        iVideoWidth = parsed_width;
        iVideoHeight = parsed_height;
        iVideoFps = parsed_fps;
        return true;
    } catch (...) {
        return false;
    }
}

bool getAVCInfo(const string &strSps, int &iVideoWidth, int &iVideoHeight, float &iVideoFps) {
    return getAVCInfo(strSps.data(), strSps.size(), iVideoWidth, iVideoHeight, iVideoFps);
}

static const char *memfind(const char *buf, ssize_t len, const char *subbuf, ssize_t sublen) {
    for (auto i = 0; i < len - sublen; ++i) {
        if (memcmp(buf + i, subbuf, sublen) == 0) {
            return buf + i;
        }
    }
    return NULL;
}

void splitH264(
    const char *ptr, size_t len, size_t prefix, const std::function<void(const char *, size_t, size_t)> &cb) {
    auto start = ptr + prefix;
    auto end = ptr + len;
    size_t next_prefix;
    while (true) {
        auto next_start = memfind(start, end - start, "\x00\x00\x01", 3);
        if (next_start) {
            // 找到下一帧  [AUTO-TRANSLATED:7161f54a]
            // Find the next frame
            if (*(next_start - 1) == 0x00) {
                // 这个是00 00 00 01开头  [AUTO-TRANSLATED:b0d79e9e]
                // This starts with 00 00 00 01
                next_start -= 1;
                next_prefix = 4;
            } else {
                // 这个是00 00 01开头  [AUTO-TRANSLATED:18ae81d8]
                // This starts with 00 00 01
                next_prefix = 3;
            }
            // 记得加上本帧prefix长度  [AUTO-TRANSLATED:8bde5d52]
            // Remember to add the prefix length of this frame
            cb(start - prefix, next_start - start + prefix, prefix);
            // 搜索下一帧末尾的起始位置  [AUTO-TRANSLATED:8976b719]
            // Search for the starting position of the end of the next frame
            start = next_start + next_prefix;
            // 记录下一帧的prefix长度  [AUTO-TRANSLATED:756aee4e]
            // Record the prefix length of the next frame
            prefix = next_prefix;
            continue;
        }
        // 未找到下一帧,这是最后一帧  [AUTO-TRANSLATED:58365453]
        // The next frame was not found, this is the last frame
        cb(start - prefix, end - start + prefix, prefix);
        break;
    }
}

size_t prefixSize(const char *ptr, size_t len) {
    if (len < 4) {
        return 0;
    }

    if (ptr[0] != 0x00 || ptr[1] != 0x00) {
        // 不是0x00 00开头  [AUTO-TRANSLATED:c406f0da]
        // Not 0x00 00 at the beginning
        return 0;
    }

    if (ptr[2] == 0x00 && ptr[3] == 0x01) {
        // 是0x00 00 00 01  [AUTO-TRANSLATED:70caae72]
        // It is 0x00 00 00 01
        return 4;
    }

    if (ptr[2] == 0x01) {
        // 是0x00 00 01  [AUTO-TRANSLATED:78b4a3c9]
        // It is 0x00 00 01
        return 3;
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

H264Track::H264Track(const string &sps, const string &pps, int sps_prefix_len, int pps_prefix_len) {
    _sps = sps.substr(sps_prefix_len);
    _pps = pps.substr(pps_prefix_len);
    H264Track::update();
}

CodecId H264Track::getCodecId() const {
    return CodecH264;
}

int H264Track::getVideoHeight() const {
    return _height;
}

int H264Track::getVideoWidth() const {
    return _width;
}

float H264Track::getVideoFps() const {
    return _fps;
}

bool H264Track::ready() const {
    return !_sps.empty() && !_pps.empty();
}

bool H264Track::inputFrame(const Frame::Ptr &frame) {
    using H264FrameInternal = FrameInternal<H264FrameNoCacheAble>;
    int type = H264_TYPE(frame->data()[frame->prefixSize()]);
    if ((type == H264Frame::NAL_B_P || type == H264Frame::NAL_IDR) && ready()) {
        return inputFrame_l(frame);
    }

    // 非I/B/P帧情况下，split一下，防止多个帧粘合在一起  [AUTO-TRANSLATED:b69c6e75]
    // In the case of non-I/B/P frames, split it to prevent multiple frames from sticking together
    bool ret = false;
    splitH264(frame->data(), frame->size(), frame->prefixSize(), [&](const char *ptr, size_t len, size_t prefix) {
        H264FrameInternal::Ptr sub_frame = std::make_shared<H264FrameInternal>(frame, (char *)ptr, len, prefix);
        if (inputFrame_l(sub_frame)) {
            ret = true;
        }
    });
    return ret;
}

toolkit::Buffer::Ptr H264Track::getExtraData() const {
    CHECK(ready());

#ifdef ENABLE_MP4
    struct mpeg4_avc_t avc;
    memset(&avc, 0, sizeof(avc));
    // mpeg4_avc_t 使用固定数组保存 SPS/PPS，第三方转换器在总长度超限时会触发断言；这里用减法检查避免加法溢出，并在进入转换器前正常失败。
    // mpeg4_avc_t stores SPS/PPS in a fixed array and its converter asserts when their total size exceeds it; subtraction-based checks avoid addition overflow and fail cleanly first.
    if (_sps.size() > sizeof(avc.data) || _pps.size() > sizeof(avc.data) - _sps.size()) {
        WarnL << "H264参数集过大，无法生成extra_data: sps=" << _sps.size() << ", pps=" << _pps.size()
              << ", capacity=" << sizeof(avc.data);
        return nullptr;
    }
    // 第三方转换器用项目 assert 宏报告参数集语法错误，该宏会抛出 AssertFailedException；仅检查长度无法覆盖内容截断的 Exp-Golomb 编码，因此只在 Track 边界收口第三方调用并维持返回 nullptr 的失败语义，其他异常仍正常传播。
    // The third-party converter reports parameter-set syntax errors through the project assert macro, which throws AssertFailedException; length checks cannot cover truncated Exp-Golomb content, so only third-party calls are contained at the Track boundary to preserve the nullptr failure contract while other exceptions still propagate.
    try {
        string sps_pps = string("\x00\x00\x00\x01", 4) + _sps + string("\x00\x00\x00\x01", 4) + _pps;
        // annexbtomp4 在仅填充配置、没有媒体输出缓冲区时固定返回 0；from_nalu 是库为该场景提供的封装，并会确认 SPS/PPS 已写入 avc。
        // annexbtomp4 always returns zero when only populating configuration without a media output buffer; from_nalu wraps that use case and verifies SPS/PPS were stored in avc.
        if (mpeg4_avc_from_nalu((const uint8_t *)sps_pps.data(), sps_pps.size(), &avc) <= 0) {
            WarnL << "生成H264 extra_data时转换参数集失败";
            return nullptr;
        }

        // 固定的 1024 字节缓冲区小于 mpeg4_avc_t 可保存的参数集；按输入大小分配，并为 AVC 配置记录字段保留充足空间。
        // A fixed 1024-byte buffer is smaller than the parameter sets held by mpeg4_avc_t; size it from the input and leave ample room for AVC record fields.
        std::string extra_data;
        extra_data.resize(sps_pps.size() + 64);
        auto extra_data_size = mpeg4_avc_decoder_configuration_record_save(&avc, (uint8_t *)&extra_data[0], extra_data.size());
        if (extra_data_size <= 0) {
            WarnL << "生成H264 extra_data 失败";
            return nullptr;
        }
        extra_data.resize(extra_data_size);
        return std::make_shared<BufferString>(std::move(extra_data));
    } catch (const AssertFailedException &ex) {
        WarnL << "生成H264 extra_data时参数集无效: " << ex.what();
        return nullptr;
    }
#else
    // AVCDecoderConfigurationRecord 使用 16 位字段保存单个 SPS/PPS 长度；拒绝截断转换，同时保证下方读取 profile/level 字节安全。
    // AVCDecoderConfigurationRecord uses 16-bit SPS/PPS lengths; reject narrowing conversions and ensure the profile/level bytes read below are present.
    if (_sps.size() < 4 || _sps.size() > std::numeric_limits<uint16_t>::max() ||
        _pps.size() > std::numeric_limits<uint16_t>::max()) {
        WarnL << "H264参数集长度无效，无法生成extra_data: sps=" << _sps.size() << ", pps=" << _pps.size();
        return nullptr;
    }
    std::string extra_data;
    // AVCDecoderConfigurationRecord start
    extra_data.push_back(1); // version
    extra_data.push_back(_sps[1]); // profile
    extra_data.push_back(_sps[2]); // compat
    extra_data.push_back(_sps[3]); // level
    extra_data.push_back((char)0xff); // 6 bits reserved + 2 bits nal size length - 1 (11)
    extra_data.push_back((char)0xe1); // 3 bits reserved + 5 bits number of sps (00001)
    // sps
    uint16_t size = (uint16_t)_sps.size();
    size = htons(size);
    extra_data.append((char *)&size, 2);
    extra_data.append(_sps);
    // pps
    extra_data.push_back(1); // version
    size = (uint16_t)_pps.size();
    size = htons(size);
    extra_data.append((char *)&size, 2);
    extra_data.append(_pps);
    return std::make_shared<BufferString>(std::move(extra_data));
#endif
}

void H264Track::setExtraData(const uint8_t *data, size_t bytes) {
#ifdef ENABLE_MP4
    struct mpeg4_avc_t avc;
    memset(&avc, 0, sizeof(avc));
    if (mpeg4_avc_decoder_configuration_record_load(data, bytes, &avc) > 0) {
        std::vector<uint8_t> config(bytes * 2);
        int size = mpeg4_avc_to_nalu(&avc, config.data(), bytes * 2);
        if (size > 4) {
            splitH264((char *)config.data(), size, 4, [&](const char *ptr, size_t len, size_t prefix) {
                inputFrame_l(std::make_shared<H264FrameNoCacheAble>((char *)ptr, len, 0, 0, prefix));
            });
            update();
        }
    }
#else
    CHECK(bytes >= 8); // 6 + 2
    size_t offset = 6;

    uint16_t sps_size = data[offset] << 8 | data[offset + 1];
    auto sps_ptr = data + offset + 2;
    offset += (2 + sps_size);
    CHECK(bytes >= offset + 2); // + pps_size
    _sps.assign((char *)sps_ptr, sps_size);

    uint16_t pps_size = data[offset] << 8 | data[offset + 1];
    auto pps_ptr = data + offset + 2;
    offset += (2 + pps_size);
    CHECK(bytes >= offset);
    _pps.assign((char *)pps_ptr, pps_size);
    update();
#endif
}

bool H264Track::update() {
    return getAVCInfo(_sps, _width, _height, _fps);
}

std::vector<Frame::Ptr> H264Track::getConfigFrames() const {
    if (!ready()) {
        return {};
    }
    return { createConfigFrame<H264Frame>(_sps, 0, getIndex()),
             createConfigFrame<H264Frame>(_pps, 0, getIndex()) };
}

Track::Ptr H264Track::clone() const {
    return std::make_shared<H264Track>(*this);
}

bool H264Track::inputFrame_l(const Frame::Ptr &frame) {
    int type = H264_TYPE(frame->data()[frame->prefixSize()]);
    if (type == H264Frame::NAL_AUD) {
        // AUD帧丢弃
        return false;
    }
    bool was_ready = ready();
    bool ret = true;
    switch (type) {
        case H264Frame::NAL_SPS: {
            _sps = string(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
            _latest_is_sps = true;
            ret = VideoTrack::inputFrame(frame);
            break;
        }
        case H264Frame::NAL_PPS: {
            _pps = string(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
            _latest_is_pps = true;
            ret = VideoTrack::inputFrame(frame);
            break;
        }
        default:
            // 避免识别不出关键帧  [AUTO-TRANSLATED:8eb84679]
            // Avoid not being able to recognize keyframes
            if (latestIsConfigFrame() && !frame->dropAble()) {
                if (!frame->keyFrame()) {
                    const_cast<Frame::Ptr &>(frame) = std::make_shared<FrameCacheAble>(frame, true);
                }
            }
            // 判断是否是I帧, 并且如果是,那判断前面是否插入过config帧, 如果插入过就不插入了  [AUTO-TRANSLATED:40733cd8]
            // Determine if it is an I frame, and if it is, determine if a config frame has been inserted before, and if it has been inserted, do not insert it
            if (frame->keyFrame() && !latestIsConfigFrame()) {
                insertConfigFrame(frame);
            }
            if(!frame->dropAble()){
                _latest_is_pps = false;
                _latest_is_sps = false;
            }
            ret = VideoTrack::inputFrame(frame);
            break;
    }

    // 仅当 SPS 改变或本帧首次补齐配置时重试：PPS 不包含宽高，配置已齐全后重复 PPS 只会反复解析同一份失败 SPS。
    // Retry only when the SPS changes or this frame first completes configuration: PPS carries no dimensions, so repeated PPS after readiness would only reparse the same failed SPS.
    bool configuration_became_ready = !was_ready && ready();
    if (_width == 0 && ready() && (type == H264Frame::NAL_SPS || configuration_became_ready)) {
        update();
    }
    return ret;
}

void H264Track::insertConfigFrame(const Frame::Ptr &frame) {
    if (!_sps.empty()) {
        VideoTrack::inputFrame(createConfigFrame<H264Frame>(_sps, frame->dts(), frame->getIndex()));
    }

    if (!_pps.empty()) {
        VideoTrack::inputFrame(createConfigFrame<H264Frame>(_pps, frame->dts(), frame->getIndex()));
    }
}

bool H264Track::latestIsConfigFrame(){
    return _latest_is_sps && _latest_is_pps;
}

class H264Sdp : public Sdp {
public:
    H264Sdp(const string &strSPS, const string &strPPS, int payload_type, int bitrate) : Sdp(90000, payload_type) {
        _printer << "m=video 0 RTP/AVP " << payload_type << "\r\n";
        if (bitrate) {
            _printer << "b=AS:" << bitrate << "\r\n";
        }
        _printer << "a=rtpmap:" << payload_type << " " << getCodecName(CodecH264) << "/" << 90000 << "\r\n";

        /**
         Single NAI Unit Mode = 0. // Single NAI mode (Only nals from 1-23 are allowed)
         Non Interleaved Mode = 1，// Non-interleaved Mode: 1-23，24 (STAP-A)，28 (FU-A) are allowed
         Interleaved Mode = 2,  // 25 (STAP-B)，26 (MTAP16)，27 (MTAP24)，28 (EU-A)，and 29 (EU-B) are allowed.
         Single NAI Unit Mode = 0. // Single NAI mode (Only nals from 1-23 are allowed)
         Non Interleaved Mode = 1，// Non-interleaved Mode: 1-23，24 (STAP-A)，28 (FU-A) are allowed
         Interleaved Mode = 2,  // 25 (STAP-B)，26 (MTAP16)，27 (MTAP24)，28 (EU-A)，and 29 (EU-B) are allowed.
         *
         * [AUTO-TRANSLATED:6166738f]
         **/
        GET_CONFIG(bool, h264_stap_a, Rtp::kH264StapA);
        _printer << "a=fmtp:" << payload_type << " packetization-mode=" << h264_stap_a << "; profile-level-id=";

        uint32_t profile_level_id = 0;
        if (strSPS.length() >= 4) { // sanity check
            profile_level_id = (uint8_t(strSPS[1]) << 16) |
                               (uint8_t(strSPS[2]) << 8) |
                               (uint8_t(strSPS[3])); // profile_idc|constraint_setN_flag|level_idc
        }

        char profile[8];
        snprintf(profile, sizeof(profile), "%06X", profile_level_id);
        _printer << profile;
        _printer << "; sprop-parameter-sets=";
        _printer << encodeBase64(strSPS) << ",";
        _printer << encodeBase64(strPPS) << "\r\n";
    }

    string getSdp() const { return _printer; }

private:
    _StrPrinter _printer;
};

Sdp::Ptr H264Track::getSdp(uint8_t payload_type) const {
    return std::make_shared<H264Sdp>(_sps, _pps, payload_type, getBitRate() >> 10);
}

namespace {

CodecId getCodec() {
    return CodecH264;
}

Track::Ptr getTrackByCodecId(int sample_rate, int channels, int sample_bit) {
    return std::make_shared<H264Track>();
}

Track::Ptr getTrackBySdp(const SdpTrack::Ptr &track) {
    //a=fmtp:96 packetization-mode=1;profile-level-id=42C01F;sprop-parameter-sets=Z0LAH9oBQBboQAAAAwBAAAAPI8YMqA==,aM48gA==
    auto map = Parser::parseArgs(track->_fmtp, ";", "=");
    auto sps_pps = map["sprop-parameter-sets"];
    string base64_SPS = findSubString(sps_pps.data(), NULL, ",");
    string base64_PPS = findSubString(sps_pps.data(), ",", NULL);
    auto sps = decodeBase64(base64_SPS);
    auto pps = decodeBase64(base64_PPS);
    if (sps.empty() || pps.empty()) {
        // 如果sdp里面没有sps/pps,那么可能在后续的rtp里面恢复出sps/pps  [AUTO-TRANSLATED:60f03d45]
        // If there is no sps/pps in the sdp, then it may be possible to recover the sps/pps in the subsequent rtp
        return std::make_shared<H264Track>();
    }
    return std::make_shared<H264Track>(sps, pps, prefixSize(sps.data(), sps.size()), prefixSize(pps.data(), pps.size()));
}

RtpCodec::Ptr getRtpEncoderByCodecId(uint8_t pt) {
    return std::make_shared<H264RtpEncoder>();
}

RtpCodec::Ptr getRtpDecoderByCodecId() {
    return std::make_shared<H264RtpDecoder>();
}

RtmpCodec::Ptr getRtmpEncoderByTrack(const Track::Ptr &track) {
    return std::make_shared<H264RtmpEncoder>(track);
}

RtmpCodec::Ptr getRtmpDecoderByTrack(const Track::Ptr &track) {
    return std::make_shared<H264RtmpDecoder>(track);
}

Frame::Ptr getFrameFromPtr(const char *data, size_t bytes, uint64_t dts, uint64_t pts) {
    return std::make_shared<H264FrameNoCacheAble>((char *)data, bytes, dts, pts, prefixSize(data, bytes));
}

} // namespace

CodecPlugin h264_plugin = { getCodec,
                            getTrackByCodecId,
                            getTrackBySdp,
                            getRtpEncoderByCodecId,
                            getRtpDecoderByCodecId,
                            getRtmpEncoderByTrack,
                            getRtmpDecoderByTrack,
                            getFrameFromPtr };

} // namespace mediakit
