/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of this source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of this source tree.
 */

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace mediakit {
bool getAVCInfo(const string &strSps, int &width, int &height, float &fps);
bool getHEVCInfo(const string &strVps, const string &strSps, int &width, int &height, float &fps);
} // namespace mediakit

namespace {

void expect(bool condition, const string &message) {
    if (!condition) {
        throw runtime_error(message);
    }
}

string fromHex(const string &hex) {
    expect((hex.size() & 1) == 0, "hex fixture must contain complete bytes");
    string result;
    result.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        result.push_back((char)strtoul(hex.substr(i, 2).c_str(), nullptr, 16));
    }
    return result;
}

class BitWriter {
public:
    void writeBit(bool bit) {
        if ((_bit_pos & 7) == 0) {
            _data.emplace_back(0);
        }
        if (bit) {
            _data.back() |= (uint8_t)(1U << (7 - (_bit_pos & 7)));
        }
        ++_bit_pos;
    }

    void writeBits(uint64_t value, size_t bits) {
        for (size_t i = bits; i > 0; --i) {
            writeBit(((value >> (i - 1)) & 1) != 0);
        }
    }

    void writeUE(uint32_t value) {
        uint64_t encoded = (uint64_t)value + 1;
        size_t bits = 0;
        for (uint64_t value_copy = encoded; value_copy; value_copy >>= 1) {
            ++bits;
        }
        for (size_t i = 1; i < bits; ++i) {
            writeBit(false);
        }
        writeBits(encoded, bits);
    }

    void writeSE(int32_t value) {
        uint32_t encoded = value > 0 ? (uint32_t)((uint64_t)value * 2 - 1) : (uint32_t)(-(int64_t)value * 2);
        writeUE(encoded);
    }

    vector<uint8_t> finishRbsp() {
        writeBit(true);
        while (_bit_pos & 7) {
            writeBit(false);
        }
        return _data;
    }

private:
    vector<uint8_t> _data;
    size_t _bit_pos = 0;
};

string makeNalu(const vector<uint8_t> &header, vector<uint8_t> rbsp) {
    string nalu((const char *)header.data(), header.size());
    unsigned zero_count = 0;
    for (auto byte : rbsp) {
        if (zero_count >= 2 && byte <= 3) {
            nalu.push_back('\x03');
            zero_count = 0;
        }
        nalu.push_back((char)byte);
        zero_count = byte == 0 ? zero_count + 1 : 0;
    }
    return nalu;
}

void writeH264Prefix(BitWriter &writer, uint8_t profile) {
    writer.writeBits(profile, 8);
    writer.writeBits(0, 8); // constraint flags + reserved_zero_2bits
    writer.writeBits(31, 8); // level_idc
    writer.writeUE(0); // seq_parameter_set_id
}

void writeH264DimensionsAndTail(BitWriter &writer) {
    writer.writeUE(1); // max_num_ref_frames
    writer.writeBit(false); // gaps_in_frame_num_value_allowed_flag
    writer.writeUE(79); // pic_width_in_mbs_minus1: 1280 / 16 - 1
    writer.writeUE(44); // pic_height_in_map_units_minus1: 720 / 16 - 1
    writer.writeBit(true); // frame_mbs_only_flag
    writer.writeBit(true); // direct_8x8_inference_flag
    writer.writeBit(false); // frame_cropping_flag
    writer.writeBit(false); // vui_parameters_present_flag
}

string makeH264PocCycleSps(uint32_t cycle_count) {
    BitWriter writer;
    writeH264Prefix(writer, 66);
    writer.writeUE(0); // log2_max_frame_num_minus4
    writer.writeUE(1); // pic_order_cnt_type
    writer.writeBit(false); // delta_pic_order_always_zero_flag
    writer.writeSE(0); // offset_for_non_ref_pic
    writer.writeSE(0); // offset_for_top_to_bottom_field
    writer.writeUE(cycle_count);
    for (uint32_t i = 0; i < cycle_count; ++i) {
        writer.writeSE(0);
    }
    writeH264DimensionsAndTail(writer);
    return makeNalu({ 0x67 }, writer.finishRbsp());
}

string makeH264ExtremeScalingSps() {
    BitWriter writer;
    writeH264Prefix(writer, 100);
    writer.writeUE(1); // chroma_format_idc
    writer.writeUE(0); // bit_depth_luma_minus8
    writer.writeUE(0); // bit_depth_chroma_minus8
    writer.writeBit(false); // qpprime_y_zero_transform_bypass_flag
    writer.writeBit(true); // seq_scaling_matrix_present_flag
    writer.writeBit(true); // seq_scaling_list_present_flag[0]
    writer.writeSE(INT32_MAX);
    for (unsigned i = 1; i < 16; ++i) {
        writer.writeSE(0);
    }
    for (unsigned i = 1; i < 8; ++i) {
        writer.writeBit(false);
    }
    writer.writeUE(0); // log2_max_frame_num_minus4
    writer.writeUE(0); // pic_order_cnt_type
    writer.writeUE(0); // log2_max_pic_order_cnt_lsb_minus4
    writeH264DimensionsAndTail(writer);
    return makeNalu({ 0x67 }, writer.finishRbsp());
}

void writeH265ProfileTierLevel(BitWriter &writer) {
    writer.writeBits(0, 8); // profile_space + tier_flag + profile_idc
    writer.writeBits(0, 32); // profile_compatibility_flag
    writer.writeBits(0, 4); // source and packing constraint flags
    writer.writeBits(0, 44); // reserved_zero_44bits
    writer.writeBits(0, 8); // general_level_idc
}

void writeH265Prefix(BitWriter &writer, bool conformance_window, uint32_t crop_left = 0) {
    writer.writeBits(0, 4); // sps_video_parameter_set_id
    writer.writeBits(0, 3); // sps_max_sub_layers_minus1
    writer.writeBit(true); // sps_temporal_id_nesting_flag
    writeH265ProfileTierLevel(writer);
    writer.writeUE(0); // sps_seq_parameter_set_id
    writer.writeUE(1); // chroma_format_idc
    writer.writeUE(1280); // pic_width_in_luma_samples
    writer.writeUE(720); // pic_height_in_luma_samples
    writer.writeBit(conformance_window);
    if (conformance_window) {
        writer.writeUE(crop_left);
        writer.writeUE(0);
        writer.writeUE(0);
        writer.writeUE(0);
    }
    writer.writeUE(0); // bit_depth_luma_minus8
    writer.writeUE(0); // bit_depth_chroma_minus8
    writer.writeUE(0); // log2_max_pic_order_cnt_lsb_minus4
    writer.writeBit(false); // sps_sub_layer_ordering_info_present_flag
    writer.writeUE(0);
    writer.writeUE(0);
    writer.writeUE(0);
    for (unsigned i = 0; i < 6; ++i) {
        writer.writeUE(0);
    }
    writer.writeBit(false); // scaling_list_enabled_flag
    writer.writeBits(0, 2); // amp_enabled_flag + sample_adaptive_offset_enabled_flag
    writer.writeBit(false); // pcm_enabled_flag
}

void writeH265Tail(BitWriter &writer) {
    writer.writeBits(0, 2); // sps_temporal_mvp_enabled_flag + strong_intra_smoothing_enabled_flag
    writer.writeBit(false); // vui_parameters_present_flag
}

string makeH265CropSps(uint32_t crop_left) {
    BitWriter writer;
    writeH265Prefix(writer, true, crop_left);
    writer.writeUE(0); // num_short_term_ref_pic_sets
    writer.writeBit(false); // long_term_ref_pics_present_flag
    writeH265Tail(writer);
    return makeNalu({ 0x42, 0x01 }, writer.finishRbsp());
}

string makeH265ShortTermRpsSps(uint32_t count) {
    BitWriter writer;
    writeH265Prefix(writer, false);
    writer.writeUE(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (i != 0) {
            writer.writeBit(false); // inter_ref_pic_set_prediction_flag
        }
        writer.writeUE(0); // num_negative_pics
        writer.writeUE(0); // num_positive_pics
    }
    writer.writeBit(false); // long_term_ref_pics_present_flag
    writeH265Tail(writer);
    return makeNalu({ 0x42, 0x01 }, writer.finishRbsp());
}

string makeH265LongTermRefSps(uint32_t count) {
    BitWriter writer;
    writeH265Prefix(writer, false);
    writer.writeUE(0); // num_short_term_ref_pic_sets
    writer.writeBit(true); // long_term_ref_pics_present_flag
    writer.writeUE(count);
    for (uint32_t i = 0; i < count; ++i) {
        writer.writeBits(0, 4); // lt_ref_pic_poc_lsb_sps
        writer.writeBit(false); // used_by_curr_pic_lt_sps_flag
    }
    writeH265Tail(writer);
    return makeNalu({ 0x42, 0x01 }, writer.finishRbsp());
}

string makeH265VpsWithLayerSets(uint32_t count) {
    BitWriter writer;
    writer.writeBits(0, 4); // vps_video_parameter_set_id
    writer.writeBits(3, 2); // vps_reserved_three_2bits
    writer.writeBits(0, 6); // vps_max_layers_minus1
    writer.writeBits(0, 3); // vps_max_sub_layers_minus1
    writer.writeBit(true); // vps_temporal_id_nesting_flag
    writer.writeBits(0xFFFF, 16); // vps_reserved_0xffff_16bits
    writeH265ProfileTierLevel(writer);
    writer.writeBit(false); // vps_sub_layer_ordering_info_present_flag
    writer.writeUE(0);
    writer.writeUE(0);
    writer.writeUE(0);
    writer.writeBits(0, 6); // vps_max_layer_id
    writer.writeUE(count); // vps_num_layer_sets_minus1
    for (uint32_t i = 0; i < count; ++i) {
        writer.writeBit(false); // layer_id_included_flag[i][0]
    }
    writer.writeBit(true); // vps_timing_info_present_flag
    writer.writeBits(1, 32); // vps_num_units_in_tick
    writer.writeBits(60, 32); // vps_time_scale
    return makeNalu({ 0x40, 0x01 }, writer.finishRbsp());
}

const string &validH264Sps() {
    static const string value = fromHex("6742c01fda014016e840000003004000000f23c60ca8");
    return value;
}

const string &validHighProfileH264Sps() {
    static const string value = fromHex("6764001facd9405005ba6a021a0280000003008000001e478c18cb");
    return value;
}

const string &validH265Vps() {
    static const string value = fromHex("40010c01ffff016000000300b0000003000003005d15c090");
    return value;
}

const string &validH265Sps() {
    static const string value = fromHex("420101016000000300b0000003000003005da005a20050162057b9165440");
    return value;
}

void testH264Basic() {
    int width = 0;
    int height = 0;
    float fps = 0;
    expect(mediakit::getAVCInfo(validH264Sps(), width, height, fps), "valid H264 SPS should parse");
    expect(width == 1280 && height == 720, "valid H264 SPS dimensions should remain unchanged");

    width = 1920;
    height = 1080;
    fps = 30;
    expect(!mediakit::getAVCInfo(validH264Sps().substr(0, 4), width, height, fps),
           "truncated H264 SPS must not reuse caller output as success");
    expect(width == 1920 && height == 1080 && fps == 30,
           "failed H264 parsing must preserve caller outputs");

    width = height = 0;
    fps = 0;
    expect(!mediakit::getAVCInfo(validH264Sps().substr(0, 9), width, height, fps),
           "H264 SPS truncated while parsing fields after dimensions must fail");

    for (uint8_t profile : { (uint8_t)138, (uint8_t)144 }) {
        auto sps = validHighProfileH264Sps();
        sps[1] = (char)profile;
        width = height = 0;
        fps = 0;
        expect(mediakit::getAVCInfo(sps, width, height, fps), "H264 profile 138/144 SPS should parse");
        expect(width == 1280 && height == 720, "H264 profile 138/144 must use high-profile syntax");
    }

    // 使用项目已有 SDP 示例防止安全加固误伤常见 profile 和 VUI 组合。
    // Reuse existing SDP examples to ensure hardening preserves common profile and VUI combinations.
    const vector<string> compatibility_samples = {
        fromHex("674d40339a6401e0021ffe02dc04040500000303e800007530e860009ffc00013ff22ef2e3430004ffe00009ff9177970a00"),
        fromHex("6742800d888b50b04b42000023280002bf20080000000001"),
        fromHex("674d001f9e35c0a00b74dc04040500000303e80000c35084"),
        fromHex("6742c01ed903c56840000003004000000c03c58b9200000001"),
        validHighProfileH264Sps(),
    };
    for (const auto &sps : compatibility_samples) {
        width = height = 0;
        fps = 0;
        expect(mediakit::getAVCInfo(sps, width, height, fps), "existing H264 SDP sample should keep parsing");
        expect(width > 0 && height > 0, "existing H264 SDP sample should produce positive dimensions");
    }
}

void testH264Limits() {
    int width = 0;
    int height = 0;
    float fps = 0;
    expect(!mediakit::getAVCInfo(makeH264PocCycleSps(256), width, height, fps),
           "H264 POC cycle count above the standard limit must fail");

    width = height = 0;
    fps = 0;
    expect(mediakit::getAVCInfo(makeH264ExtremeScalingSps(), width, height, fps),
           "extreme H264 scaling-list delta must be handled without signed overflow");
    expect(width == 1280 && height == 720, "H264 scaling-list parsing must stay aligned");
}

void testH265() {
    int width = 0;
    int height = 0;
    float fps = 0;
    expect(mediakit::getHEVCInfo(validH265Vps(), validH265Sps(), width, height, fps),
           "valid H265 SPS should parse");
    expect(width == 720 && height == 1280, "valid H265 SPS dimensions should remain unchanged");

    width = 1920;
    height = 1080;
    fps = 30;
    expect(!mediakit::getHEVCInfo(validH265Vps(), validH265Sps().substr(0, 24), width, height, fps),
           "H265 SPS truncated while parsing fields after dimensions must fail");
    expect(width == 1920 && height == 1080 && fps == 30,
           "failed H265 parsing must preserve caller outputs");

    const string compatibility_sps = fromHex(
        "420101016000000300b0000003000003005da00280802d1636b924cbf0080000030008000003019508");
    width = height = 0;
    fps = 0;
    expect(mediakit::getHEVCInfo(validH265Vps(), compatibility_sps, width, height, fps),
           "existing H265 SDP sample should keep parsing");
    expect(width > 0 && height > 0, "existing H265 SDP sample should produce positive dimensions");

    width = height = 0;
    fps = 0;
    expect(!mediakit::getHEVCInfo("", makeH265CropSps(0x80000000U), width, height, fps),
           "H265 crop multiplication overflow must fail");

    expect(!mediakit::getHEVCInfo("", makeH265ShortTermRpsSps(65), width, height, fps),
           "H265 short-term RPS count above the standard limit must fail");
    expect(!mediakit::getHEVCInfo("", makeH265LongTermRefSps(33), width, height, fps),
           "H265 long-term reference count above the standard limit must fail");

    width = height = 0;
    fps = 0;
    expect(mediakit::getHEVCInfo(makeH265VpsWithLayerSets(1024), validH265Sps(), width, height, fps),
           "an invalid VPS must not prevent valid SPS dimensions from being extracted");
    expect(fps == 0, "VPS layer-set count above the standard limit must be rejected before publishing timing");
}

} // namespace

int main(int argc, char **argv) {
    try {
        const string group = argc > 1 ? argv[1] : "all";
        if (group == "all" || group == "h264-basic") {
            testH264Basic();
        }
        if (group == "all" || group == "h264-limits") {
            testH264Limits();
        }
        if (group == "all" || group == "h265") {
            testH265();
        }
        expect(group == "all" || group == "h264-basic" || group == "h264-limits" || group == "h265",
               "unknown test group");
        cout << "test_sps_parser passed" << endl;
        return 0;
    } catch (const exception &ex) {
        cerr << "test_sps_parser failed: " << ex.what() << endl;
        return EXIT_FAILURE;
    }
}
