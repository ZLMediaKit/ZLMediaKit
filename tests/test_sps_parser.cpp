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
#include <climits>
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
}

struct H264SpsOptions {
    uint8_t profile = 66;
    uint32_t sps_id = 0;
    uint32_t chroma_format_idc = 1;
    uint32_t bit_depth_luma_minus8 = 0;
    uint32_t bit_depth_chroma_minus8 = 0;
    uint32_t log2_max_frame_num_minus4 = 0;
    uint32_t pic_order_cnt_type = 0;
    uint32_t log2_max_pic_order_cnt_lsb_minus4 = 0;
    uint32_t poc_cycle_count = 0;
    uint32_t max_num_ref_frames = 1;
    uint32_t pic_width_in_mbs_minus1 = 79;
    uint32_t pic_height_in_map_units_minus1 = 44;
    bool frame_mbs_only = true;
};

bool usesH264ExtendedSyntax(uint8_t profile) {
    return profile == 100 || profile == 110 || profile == 122 || profile == 244 || profile == 44 ||
           profile == 83 || profile == 86 || profile == 118 || profile == 128 || profile == 138 || profile == 144;
}

string makeH264Sps(const H264SpsOptions &options) {
    BitWriter writer;
    writeH264Prefix(writer, options.profile);
    writer.writeUE(options.sps_id);
    if (usesH264ExtendedSyntax(options.profile)) {
        writer.writeUE(options.chroma_format_idc);
        if (options.chroma_format_idc == 3) {
            writer.writeBit(false); // separate_colour_plane_flag
        }
        writer.writeUE(options.bit_depth_luma_minus8);
        writer.writeUE(options.bit_depth_chroma_minus8);
        writer.writeBit(false); // qpprime_y_zero_transform_bypass_flag
        writer.writeBit(false); // seq_scaling_matrix_present_flag
    }
    writer.writeUE(options.log2_max_frame_num_minus4);
    writer.writeUE(options.pic_order_cnt_type);
    if (options.pic_order_cnt_type == 0) {
        writer.writeUE(options.log2_max_pic_order_cnt_lsb_minus4);
    } else if (options.pic_order_cnt_type == 1) {
        writer.writeBit(false); // delta_pic_order_always_zero_flag
        writer.writeSE(0); // offset_for_non_ref_pic
        writer.writeSE(0); // offset_for_top_to_bottom_field
        writer.writeUE(options.poc_cycle_count);
        for (uint32_t i = 0; i < options.poc_cycle_count; ++i) {
            writer.writeSE(0);
        }
    }
    writer.writeUE(options.max_num_ref_frames);
    writer.writeBit(false); // gaps_in_frame_num_value_allowed_flag
    writer.writeUE(options.pic_width_in_mbs_minus1);
    writer.writeUE(options.pic_height_in_map_units_minus1);
    writer.writeBit(options.frame_mbs_only);
    if (!options.frame_mbs_only) {
        writer.writeBit(false); // mb_adaptive_frame_field_flag
    }
    writer.writeBit(true); // direct_8x8_inference_flag
    writer.writeBit(false); // frame_cropping_flag
    writer.writeBit(false); // vui_parameters_present_flag
    return makeNalu({ 0x67 }, writer.finishRbsp());
}

string makeH264PocCycleSps(uint32_t cycle_count) {
    H264SpsOptions options;
    options.pic_order_cnt_type = 1;
    options.poc_cycle_count = cycle_count;
    return makeH264Sps(options);
}

string makeH264ScalingSps(uint32_t first_delta_code_num) {
    BitWriter writer;
    writeH264Prefix(writer, 100);
    writer.writeUE(0); // seq_parameter_set_id
    writer.writeUE(1); // chroma_format_idc
    writer.writeUE(0); // bit_depth_luma_minus8
    writer.writeUE(0); // bit_depth_chroma_minus8
    writer.writeBit(false); // qpprime_y_zero_transform_bypass_flag
    writer.writeBit(true); // seq_scaling_matrix_present_flag
    writer.writeBit(true); // seq_scaling_list_present_flag[0]
    writer.writeUE(first_delta_code_num);
    for (unsigned i = 1; i < 16; ++i) {
        writer.writeSE(0);
    }
    for (unsigned i = 1; i < 8; ++i) {
        writer.writeBit(false);
    }
    writer.writeUE(0); // log2_max_frame_num_minus4
    writer.writeUE(0); // pic_order_cnt_type
    writer.writeUE(0); // log2_max_pic_order_cnt_lsb_minus4
    writer.writeUE(1); // max_num_ref_frames
    writer.writeBit(false); // gaps_in_frame_num_value_allowed_flag
    writer.writeUE(79); // pic_width_in_mbs_minus1
    writer.writeUE(44); // pic_height_in_map_units_minus1
    writer.writeBit(true); // frame_mbs_only_flag
    writer.writeBit(true); // direct_8x8_inference_flag
    writer.writeBit(false); // frame_cropping_flag
    writer.writeBit(false); // vui_parameters_present_flag
    return makeNalu({ 0x67 }, writer.finishRbsp());
}

void writeH265ProfileTierLevel(BitWriter &writer, uint32_t max_sub_layers_minus1 = 0) {
    writer.writeBits(0, 8); // profile_space + tier_flag + profile_idc
    writer.writeBits(0, 32); // profile_compatibility_flag
    writer.writeBits(0, 4); // source and packing constraint flags
    writer.writeBits(0, 44); // reserved_zero_44bits
    writer.writeBits(0, 8); // general_level_idc
    for (uint32_t i = 0; i < max_sub_layers_minus1; ++i) {
        writer.writeBits(0, 2); // sub_layer_profile/level_present_flag
    }
    if (max_sub_layers_minus1 > 0) {
        for (uint32_t i = max_sub_layers_minus1; i < 8; ++i) {
            writer.writeBits(0, 2); // reserved_zero_2bits
        }
    }
}

struct H265SpsOptions {
    uint32_t max_sub_layers_minus1 = 0;
    uint32_t sps_id = 0;
    uint32_t chroma_format_idc = 1;
    uint32_t pic_width = 1280;
    uint32_t pic_height = 720;
    bool conformance_window = false;
    uint32_t crop_left = 0;
    uint32_t bit_depth_luma_minus8 = 0;
    uint32_t bit_depth_chroma_minus8 = 0;
    uint32_t log2_max_pic_order_cnt_lsb_minus4 = 0;
    uint32_t num_short_term_ref_pic_sets = 0;
    uint32_t first_num_negative_pics = 0;
    uint32_t first_num_positive_pics = 0;
    uint32_t num_long_term_ref_pics = 0;
};

void writeH265Prefix(BitWriter &writer, const H265SpsOptions &options) {
    writer.writeBits(0, 4); // sps_video_parameter_set_id
    writer.writeBits(options.max_sub_layers_minus1, 3);
    writer.writeBit(true); // sps_temporal_id_nesting_flag
    writeH265ProfileTierLevel(writer, options.max_sub_layers_minus1);
    writer.writeUE(options.sps_id);
    writer.writeUE(options.chroma_format_idc);
    if (options.chroma_format_idc == 3) {
        writer.writeBit(false); // separate_colour_plane_flag
    }
    writer.writeUE(options.pic_width);
    writer.writeUE(options.pic_height);
    writer.writeBit(options.conformance_window);
    if (options.conformance_window) {
        writer.writeUE(options.crop_left);
        writer.writeUE(0);
        writer.writeUE(0);
        writer.writeUE(0);
    }
    writer.writeUE(options.bit_depth_luma_minus8);
    writer.writeUE(options.bit_depth_chroma_minus8);
    writer.writeUE(options.log2_max_pic_order_cnt_lsb_minus4);
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

string makeH265Sps(const H265SpsOptions &options) {
    BitWriter writer;
    writeH265Prefix(writer, options);
    writer.writeUE(options.num_short_term_ref_pic_sets);
    for (uint32_t i = 0; i < options.num_short_term_ref_pic_sets; ++i) {
        if (i != 0) {
            writer.writeBit(false); // inter_ref_pic_set_prediction_flag
        }
        uint32_t num_negative = i == 0 ? options.first_num_negative_pics : 0;
        uint32_t num_positive = i == 0 ? options.first_num_positive_pics : 0;
        writer.writeUE(num_negative);
        writer.writeUE(num_positive);
        for (uint32_t j = 0; j < num_negative; ++j) {
            writer.writeUE(0);
            writer.writeBit(false);
        }
        for (uint32_t j = 0; j < num_positive; ++j) {
            writer.writeUE(0);
            writer.writeBit(false);
        }
    }
    writer.writeBit(options.num_long_term_ref_pics != 0);
    if (options.num_long_term_ref_pics != 0) {
        writer.writeUE(options.num_long_term_ref_pics);
        uint32_t poc_bits = options.log2_max_pic_order_cnt_lsb_minus4 + 4;
        for (uint32_t i = 0; i < options.num_long_term_ref_pics; ++i) {
            writer.writeBits(0, poc_bits);
            writer.writeBit(false);
        }
    }
    writeH265Tail(writer);
    return makeNalu({ 0x42, 0x01 }, writer.finishRbsp());
}

string makeH265CropSps(uint32_t crop_left) {
    H265SpsOptions options;
    options.conformance_window = true;
    options.crop_left = crop_left;
    return makeH265Sps(options);
}

string makeH265ShortTermRpsSps(uint32_t count) {
    H265SpsOptions options;
    options.num_short_term_ref_pic_sets = count;
    return makeH265Sps(options);
}

string makeH265LongTermRefSps(uint32_t count) {
    H265SpsOptions options;
    options.num_long_term_ref_pics = count;
    return makeH265Sps(options);
}

string makeH265VpsWithLayerSets(uint32_t count, uint32_t max_sub_layers_minus1 = 0, uint32_t max_layer_id = 0) {
    BitWriter writer;
    writer.writeBits(0, 4); // vps_video_parameter_set_id
    writer.writeBits(3, 2); // vps_reserved_three_2bits
    writer.writeBits(max_layer_id, 6); // vps_max_layers_minus1
    writer.writeBits(max_sub_layers_minus1, 3);
    writer.writeBit(true); // vps_temporal_id_nesting_flag
    writer.writeBits(0xFFFF, 16); // vps_reserved_0xffff_16bits
    writeH265ProfileTierLevel(writer, max_sub_layers_minus1);
    writer.writeBit(false); // vps_sub_layer_ordering_info_present_flag
    writer.writeUE(0);
    writer.writeUE(0);
    writer.writeUE(0);
    writer.writeBits(max_layer_id, 6);
    writer.writeUE(count); // vps_num_layer_sets_minus1
    for (uint32_t i = 0; i < count; ++i) {
        for (uint32_t j = 0; j <= max_layer_id; ++j) {
            writer.writeBit(false); // layer_id_included_flag[i][j]
        }
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

    expect(mediakit::getAVCInfo(makeH264PocCycleSps(0), width, height, fps),
           "H264 POC cycle count lower bound should parse");
    expect(mediakit::getAVCInfo(makeH264PocCycleSps(1), width, height, fps),
           "H264 POC cycle count value adjacent to the lower bound should parse");
    expect(mediakit::getAVCInfo(makeH264PocCycleSps(255), width, height, fps),
           "H264 POC cycle count upper bound should parse");
    expect(!mediakit::getAVCInfo(makeH264PocCycleSps(256), width, height, fps),
           "H264 POC cycle count above the standard limit must fail");

    width = height = 0;
    fps = 0;
    expect(mediakit::getAVCInfo(makeH264ScalingSps(UINT32_MAX - 2), width, height, fps),
           "H264 se(v) positive INT32_MAX boundary should parse without signed overflow");
    expect(width == 1280 && height == 720, "positive H264 scaling-list boundary must stay aligned");
    expect(mediakit::getAVCInfo(makeH264ScalingSps(UINT32_MAX - 1), width, height, fps),
           "H264 se(v) negative INT32_MAX boundary should parse without signed overflow");
    expect(width == 1280 && height == 720, "H264 scaling-list parsing must stay aligned");
    expect(!mediakit::getAVCInfo(makeH264ScalingSps(UINT32_MAX), width, height, fps),
           "H264 ue(v) with 32 leading zero bits must be rejected");

    auto parses = [](const H264SpsOptions &options, int *parsed_width = nullptr, int *parsed_height = nullptr) {
        int width = 0;
        int height = 0;
        float fps = 0;
        bool result = mediakit::getAVCInfo(makeH264Sps(options), width, height, fps);
        if (parsed_width) {
            *parsed_width = width;
        }
        if (parsed_height) {
            *parsed_height = height;
        }
        return result;
    };

    H264SpsOptions options;
    expect(parses(options), "H264 standard-field lower bounds should parse");
    options.sps_id = 1;
    expect(parses(options), "H264 SPS id adjacent to the lower bound should parse");
    options.sps_id = 31;
    expect(parses(options), "H264 SPS id upper bound should parse");
    options.sps_id = 32;
    expect(!parses(options), "H264 SPS id above the standard limit must fail");

    options = H264SpsOptions();
    options.profile = 100;
    options.chroma_format_idc = 0;
    expect(parses(options), "H264 chroma format lower bound should parse");
    options.chroma_format_idc = 1;
    expect(parses(options), "H264 chroma format adjacent to the lower bound should parse");
    options.chroma_format_idc = 3;
    expect(parses(options), "H264 chroma format upper bound should parse");
    options.chroma_format_idc = 4;
    expect(!parses(options), "H264 chroma format above the standard limit must fail");

    options = H264SpsOptions();
    options.profile = 100;
    options.bit_depth_luma_minus8 = 1;
    options.bit_depth_chroma_minus8 = 1;
    expect(parses(options), "H264 bit depth adjacent to the lower bound should parse");
    options.bit_depth_luma_minus8 = 6;
    options.bit_depth_chroma_minus8 = 6;
    expect(parses(options), "H264 bit depth upper bound should parse");
    options.bit_depth_luma_minus8 = 7;
    options.bit_depth_chroma_minus8 = 7;
    expect(!parses(options), "H264 bit depth above the standard limit must fail");
    options.bit_depth_luma_minus8 = 0;
    options.bit_depth_chroma_minus8 = 1;
    expect(!parses(options), "H264 luma and chroma bit depths must match");

    options = H264SpsOptions();
    options.log2_max_frame_num_minus4 = 1;
    expect(parses(options), "H264 frame-number log2 value adjacent to the lower bound should parse");
    options.log2_max_frame_num_minus4 = 12;
    expect(parses(options), "H264 frame-number log2 upper bound should parse");
    options.log2_max_frame_num_minus4 = 13;
    expect(!parses(options), "H264 frame-number log2 value above the standard limit must fail");

    options = H264SpsOptions();
    options.pic_order_cnt_type = 2;
    expect(parses(options), "H264 POC type upper bound should parse");
    options.pic_order_cnt_type = 3;
    expect(!parses(options), "H264 POC type above the standard limit must fail");

    options = H264SpsOptions();
    options.log2_max_pic_order_cnt_lsb_minus4 = 1;
    expect(parses(options), "H264 POC log2 value adjacent to the lower bound should parse");
    options.log2_max_pic_order_cnt_lsb_minus4 = 12;
    expect(parses(options), "H264 POC log2 upper bound should parse");
    options.log2_max_pic_order_cnt_lsb_minus4 = 13;
    expect(!parses(options), "H264 POC log2 value above the standard limit must fail");

    options = H264SpsOptions();
    options.max_num_ref_frames = 0;
    expect(parses(options), "H264 reference-frame count lower bound should parse");
    options.max_num_ref_frames = 1;
    expect(parses(options), "H264 reference-frame count adjacent to the lower bound should parse");
    options.max_num_ref_frames = 16;
    expect(parses(options), "H264 reference-frame count upper bound should parse");
    options.max_num_ref_frames = 17;
    expect(!parses(options), "H264 reference-frame count above the standard limit must fail");

    options = H264SpsOptions();
    options.pic_width_in_mbs_minus1 = 0;
    options.pic_height_in_map_units_minus1 = 0;
    expect(parses(options, &width, &height) && width == 16 && height == 16,
           "H264 macroblock dimension lower bounds should parse");
    options.pic_width_in_mbs_minus1 = 1;
    options.pic_height_in_map_units_minus1 = 1;
    expect(parses(options, &width, &height) && width == 32 && height == 32,
           "H264 macroblock dimensions adjacent to the lower bounds should parse");
    options.pic_width_in_mbs_minus1 = INT_MAX / 16 - 1;
    options.pic_height_in_map_units_minus1 = INT_MAX / 16 - 1;
    expect(parses(options, &width, &height) && width == (INT_MAX / 16) * 16 && height == (INT_MAX / 16) * 16,
           "H264 macroblock dimensions at the int output boundary should parse");
    options.pic_width_in_mbs_minus1 = INT_MAX / 16;
    expect(!parses(options), "H264 macroblock width above the int output boundary must fail");
    options.pic_width_in_mbs_minus1 = 0;
    options.pic_height_in_map_units_minus1 = INT_MAX / 16;
    expect(!parses(options), "H264 macroblock height above the int output boundary must fail");
    options.pic_width_in_mbs_minus1 = UINT32_MAX - 1;
    options.pic_height_in_map_units_minus1 = 0;
    expect(!parses(options), "H264 31-bit ue(v) macroblock width must not wrap during plus-one derivation");
    options.pic_width_in_mbs_minus1 = UINT32_MAX;
    expect(!parses(options), "H264 32-bit ue(v) macroblock width must be rejected");
    options.pic_width_in_mbs_minus1 = 0;
    options.pic_height_in_map_units_minus1 = UINT32_MAX - 1;
    expect(!parses(options), "H264 31-bit ue(v) macroblock height must not wrap during plus-one derivation");
    options.pic_height_in_map_units_minus1 = UINT32_MAX;
    expect(!parses(options), "H264 32-bit ue(v) macroblock height must be rejected");
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

    auto parses = [](const H265SpsOptions &options, int *parsed_width = nullptr, int *parsed_height = nullptr) {
        int width = 0;
        int height = 0;
        float fps = 0;
        bool result = mediakit::getHEVCInfo("", makeH265Sps(options), width, height, fps);
        if (parsed_width) {
            *parsed_width = width;
        }
        if (parsed_height) {
            *parsed_height = height;
        }
        return result;
    };

    H265SpsOptions options;
    expect(parses(options), "H265 standard-field lower bounds should parse");
    options.max_sub_layers_minus1 = 1;
    expect(parses(options), "H265 sub-layer count adjacent to the lower bound should parse");
    options.max_sub_layers_minus1 = 6;
    expect(parses(options), "H265 sub-layer count upper bound should parse");
    options.max_sub_layers_minus1 = 7;
    expect(!parses(options), "H265 sub-layer count above the standard limit must fail");

    options = H265SpsOptions();
    options.sps_id = 1;
    expect(parses(options), "H265 SPS id adjacent to the lower bound should parse");
    options.sps_id = 15;
    expect(parses(options), "H265 SPS id upper bound should parse");
    options.sps_id = 16;
    expect(!parses(options), "H265 SPS id above the standard limit must fail");

    options = H265SpsOptions();
    options.chroma_format_idc = 0;
    expect(parses(options), "H265 chroma format lower bound should parse");
    options.chroma_format_idc = 3;
    expect(parses(options), "H265 chroma format upper bound should parse");
    options.chroma_format_idc = 4;
    expect(!parses(options), "H265 chroma format above the standard limit must fail");

    options = H265SpsOptions();
    options.bit_depth_luma_minus8 = 1;
    options.bit_depth_chroma_minus8 = 1;
    expect(parses(options), "H265 bit depth adjacent to the lower bound should parse");
    options.bit_depth_luma_minus8 = 8;
    options.bit_depth_chroma_minus8 = 8;
    expect(parses(options), "H265 bit depth upper bound should parse");
    options.bit_depth_luma_minus8 = 9;
    options.bit_depth_chroma_minus8 = 9;
    expect(!parses(options), "H265 bit depth above the standard limit must fail");
    options.bit_depth_luma_minus8 = 0;
    options.bit_depth_chroma_minus8 = 1;
    expect(!parses(options), "H265 luma and chroma bit depths must match");

    options = H265SpsOptions();
    options.log2_max_pic_order_cnt_lsb_minus4 = 1;
    expect(parses(options), "H265 POC log2 value adjacent to the lower bound should parse");
    options.log2_max_pic_order_cnt_lsb_minus4 = 12;
    expect(parses(options), "H265 POC log2 upper bound should parse");
    options.log2_max_pic_order_cnt_lsb_minus4 = 13;
    expect(!parses(options), "H265 POC log2 value above the standard limit must fail");

    options = H265SpsOptions();
    options.num_short_term_ref_pic_sets = 1;
    expect(parses(options), "H265 short-term RPS count adjacent to the lower bound should parse");
    options.num_short_term_ref_pic_sets = 64;
    expect(parses(options), "H265 short-term RPS count upper bound should parse");
    options.num_short_term_ref_pic_sets = 65;
    expect(!parses(options), "H265 short-term RPS count above the standard limit must fail");

    options = H265SpsOptions();
    options.num_short_term_ref_pic_sets = 1;
    options.first_num_negative_pics = 1;
    options.first_num_positive_pics = 1;
    expect(parses(options), "H265 negative and positive RPS counts adjacent to their lower bounds should parse");
    options.first_num_negative_pics = 15;
    options.first_num_positive_pics = 15;
    expect(parses(options), "H265 negative and positive RPS counts at their upper bounds should parse");
    options.first_num_negative_pics = 16;
    expect(!parses(options), "H265 negative RPS count above the standard limit must fail");
    options.first_num_negative_pics = 0;
    options.first_num_positive_pics = 16;
    expect(!parses(options), "H265 positive RPS count above the standard limit must fail");

    options = H265SpsOptions();
    options.num_long_term_ref_pics = 1;
    expect(parses(options), "H265 long-term reference count adjacent to the lower bound should parse");
    options.num_long_term_ref_pics = 32;
    expect(parses(options), "H265 long-term reference count upper bound should parse");
    options.num_long_term_ref_pics = 33;
    expect(!parses(options), "H265 long-term reference count above the standard limit must fail");

    options = H265SpsOptions();
    options.pic_width = 1;
    options.pic_height = 1;
    expect(parses(options, &width, &height) && width == 1 && height == 1,
           "H265 derived dimension lower bounds should parse");
    options.pic_width = 2;
    options.pic_height = 2;
    expect(parses(options, &width, &height) && width == 2 && height == 2,
           "H265 derived dimensions adjacent to the lower bounds should parse");
    options.pic_width = INT_MAX;
    options.pic_height = INT_MAX;
    expect(parses(options, &width, &height) && width == INT_MAX && height == INT_MAX,
           "H265 31-bit ue(v) dimensions at the int output boundary should parse");
    options.pic_width = (uint32_t)INT_MAX + 1;
    expect(!parses(options), "H265 width above the int output boundary must fail");
    options.pic_width = 1;
    options.pic_height = (uint32_t)INT_MAX + 1;
    expect(!parses(options), "H265 height above the int output boundary must fail");
    options.pic_width = UINT32_MAX;
    options.pic_height = 1;
    expect(!parses(options), "H265 ue(v) with 32 leading zero bits must be rejected");
    options.pic_width = 1;
    options.pic_height = UINT32_MAX;
    expect(!parses(options), "H265 32-bit ue(v) height must be rejected");

    auto parses_vps_fps = [](uint32_t layer_sets, uint32_t sub_layers, uint32_t max_layer_id) {
        int width = 0;
        int height = 0;
        float fps = 0;
        expect(mediakit::getHEVCInfo(makeH265VpsWithLayerSets(layer_sets, sub_layers, max_layer_id),
                                     validH265Sps(), width, height, fps),
               "invalid H265 VPS must not prevent valid SPS dimensions from being extracted");
        return fps;
    };
    expect(parses_vps_fps(0, 0, 0) == 60, "H265 VPS layer-set lower bound should publish timing");
    expect(parses_vps_fps(1, 1, 1) == 60, "H265 VPS fields adjacent to their lower bounds should publish timing");
    expect(parses_vps_fps(1023, 6, 62) == 60, "H265 VPS fields at their upper bounds should publish timing");
    expect(parses_vps_fps(1024, 0, 0) == 0, "H265 VPS layer-set count above the standard limit must fail");
    expect(parses_vps_fps(0, 7, 0) == 0, "H265 VPS sub-layer count above the standard limit must fail");
    expect(parses_vps_fps(0, 0, 63) == 0, "H265 VPS max-layer id above the standard limit must fail");
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
