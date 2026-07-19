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

#include "ext-codec/H264.h"
#include "ext-codec/H265.h"

#ifdef ENABLE_MP4
#include "mpeg4-avc.h"
#include "mpeg4-hevc.h"
#endif

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

    vector<uint8_t> finishRbsp(bool include_trailing_bits = true) {
        if (include_trailing_bits) {
            writeBit(true);
            while (_bit_pos & 7) {
                writeBit(false);
            }
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
    bool vui_hrd_parameters = false;
    uint32_t hrd_cpb_cnt_minus1 = 0;
    bool vcl_hrd_parameters = false;
    uint32_t vcl_hrd_cpb_cnt_minus1 = 0;
    bool include_rbsp_trailing_bits = true;
};

bool usesH264ExtendedSyntax(uint8_t profile) {
    return profile == 100 || profile == 110 || profile == 122 || profile == 244 || profile == 44 ||
           profile == 83 || profile == 86 || profile == 118 || profile == 128 || profile == 138 || profile == 144;
}

void writeH264Hrd(BitWriter &writer, uint32_t cpb_cnt_minus1) {
    writer.writeUE(cpb_cnt_minus1);
    writer.writeBits(0, 8); // bit_rate_scale + cpb_size_scale
    for (uint32_t i = 0; i <= cpb_cnt_minus1; ++i) {
        writer.writeUE(0); // bit_rate_value_minus1
        writer.writeUE(0); // cpb_size_value_minus1
        writer.writeBit(false); // cbr_flag
    }
    writer.writeBits(0, 20); // HRD delay lengths + time_offset_length
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
    writer.writeBit(options.vui_hrd_parameters);
    if (options.vui_hrd_parameters) {
        writer.writeBits(0, 4); // aspect_ratio/overscan/video_signal/chroma_loc_info_present_flag
        writer.writeBit(true); // timing_info_present_flag
        writer.writeBits(1, 32); // num_units_in_tick
        writer.writeBits(60, 32); // time_scale
        writer.writeBit(true); // fixed_frame_rate_flag
        writer.writeBit(true); // nal_hrd_parameters_present_flag
        writeH264Hrd(writer, options.hrd_cpb_cnt_minus1);
        writer.writeBit(options.vcl_hrd_parameters);
        if (options.vcl_hrd_parameters) {
            writeH264Hrd(writer, options.vcl_hrd_cpb_cnt_minus1);
        }
        writer.writeBit(false); // low_delay_hrd_flag
        writer.writeBit(false); // pic_struct_present_flag
        writer.writeBit(true); // bitstream_restriction_flag
        writer.writeBit(true); // motion_vectors_over_pic_boundaries_flag
        for (unsigned i = 0; i < 6; ++i) {
            writer.writeUE(0);
        }
    }
    return makeNalu({ 0x67 }, writer.finishRbsp(options.include_rbsp_trailing_bits));
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
    uint32_t log2_min_luma_coding_block_size_minus3 = 0;
    uint32_t log2_diff_max_min_luma_coding_block_size = 0;
    uint32_t num_short_term_ref_pic_sets = 0;
    uint32_t first_num_negative_pics = 0;
    uint32_t first_num_positive_pics = 0;
    uint32_t num_long_term_ref_pics = 0;
    bool vui_hrd_parameters = false;
    uint32_t hrd_cpb_cnt_minus1 = 0;
    bool hrd_vcl_parameters = false;
    bool hrd_sub_pic_parameters = false;
    bool hrd_fixed_pic_rate_general = true;
    bool hrd_fixed_pic_rate_within_cvs = false;
    bool hrd_low_delay = false;
    bool range_extension = false;
    bool multilayer_extension = false;
    bool extension_3d = false;
    uint32_t extension_4bits = 0;
    uint32_t ivmc_sub_pb_size_minus3 = 0;
    uint32_t texmc_sub_pb_size_minus3 = 0;
    bool scc_extension = false;
    bool palette_mode = false;
    uint32_t palette_max_size = 0;
    uint32_t delta_palette_max_predictor_size = 0;
    bool palette_initializers = false;
    uint32_t palette_initializer_count_minus1 = 0;
    uint32_t motion_vector_resolution_control_idc = 0;
    bool truncate_scc_extension = false;
    bool include_rbsp_trailing_bits = true;
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
    writer.writeUE(options.log2_min_luma_coding_block_size_minus3);
    writer.writeUE(options.log2_diff_max_min_luma_coding_block_size);
    for (unsigned i = 0; i < 4; ++i) {
        writer.writeUE(0);
    }
    writer.writeBit(false); // scaling_list_enabled_flag
    writer.writeBits(0, 2); // amp_enabled_flag + sample_adaptive_offset_enabled_flag
    writer.writeBit(false); // pcm_enabled_flag
}

void writeH265SubLayerHrd(BitWriter &writer, uint32_t cpb_cnt_minus1, bool sub_pic_parameters) {
    for (uint32_t j = 0; j <= cpb_cnt_minus1; ++j) {
        writer.writeUE(0); // bit_rate_value_minus1
        writer.writeUE(0); // cpb_size_value_minus1
        if (sub_pic_parameters) {
            writer.writeUE(0); // cpb_size_du_value_minus1
            writer.writeUE(0); // bit_rate_du_value_minus1
        }
        writer.writeBit(false); // cbr_flag
    }
}

void writeH265Tail(BitWriter &writer, const H265SpsOptions &options) {
    writer.writeBits(0, 2); // sps_temporal_mvp_enabled_flag + strong_intra_smoothing_enabled_flag
    writer.writeBit(options.vui_hrd_parameters);
    if (options.vui_hrd_parameters) {
        writer.writeBits(0, 4); // aspect_ratio/overscan/video_signal/chroma_loc_info_present_flag
        writer.writeBits(0, 3); // neutral_chroma/field_seq/frame_field_info_present_flag
        writer.writeBit(false); // default_display_window_flag
        writer.writeBit(true); // vui_timing_info_present_flag
        writer.writeBits(1, 32); // vui_num_units_in_tick
        writer.writeBits(60, 32); // vui_time_scale
        writer.writeBit(false); // vui_poc_proportional_to_timing_flag
        writer.writeBit(true); // vui_hrd_parameters_present_flag
        writer.writeBit(true); // nal_hrd_parameters_present_flag
        writer.writeBit(options.hrd_vcl_parameters);
        writer.writeBit(options.hrd_sub_pic_parameters);
        if (options.hrd_sub_pic_parameters) {
            writer.writeBits(0, 19); // sub-picture HRD lengths and flags
        }
        writer.writeBits(0, 8); // bit_rate_scale + cpb_size_scale
        if (options.hrd_sub_pic_parameters) {
            writer.writeBits(0, 4); // cpb_size_du_scale
        }
        writer.writeBits(0, 15); // HRD delay lengths
        for (uint32_t i = 0; i <= options.max_sub_layers_minus1; ++i) {
            writer.writeBit(options.hrd_fixed_pic_rate_general);
            bool fixed_pic_rate_within_cvs = options.hrd_fixed_pic_rate_general;
            if (!options.hrd_fixed_pic_rate_general) {
                writer.writeBit(options.hrd_fixed_pic_rate_within_cvs);
                fixed_pic_rate_within_cvs = options.hrd_fixed_pic_rate_within_cvs;
            }
            bool low_delay = false;
            if (fixed_pic_rate_within_cvs) {
                writer.writeUE(0); // elemental_duration_in_tc_minus1
            } else {
                writer.writeBit(options.hrd_low_delay);
                low_delay = options.hrd_low_delay;
            }
            uint32_t cpb_cnt_minus1 = low_delay ? 0 : options.hrd_cpb_cnt_minus1;
            if (!low_delay) {
                writer.writeUE(options.hrd_cpb_cnt_minus1);
            }
            writeH265SubLayerHrd(writer, cpb_cnt_minus1, options.hrd_sub_pic_parameters);
            if (options.hrd_vcl_parameters) {
                writeH265SubLayerHrd(writer, cpb_cnt_minus1, options.hrd_sub_pic_parameters);
            }
        }
        writer.writeBit(true); // bitstream_restriction_flag
        writer.writeBits(0, 3); // tiles_fixed/motion_vectors/restricted_ref_pic_lists flags
        for (unsigned i = 0; i < 5; ++i) {
            writer.writeUE(0);
        }
    }
    bool extension_present = options.range_extension || options.multilayer_extension ||
                             options.extension_3d || options.scc_extension || options.extension_4bits;
    writer.writeBit(extension_present); // sps_extension_present_flag
    if (!extension_present) {
        return;
    }

    writer.writeBit(options.range_extension);
    writer.writeBit(options.multilayer_extension);
    writer.writeBit(options.extension_3d);
    writer.writeBit(options.scc_extension);
    writer.writeBits(options.extension_4bits, 4);
    if (options.range_extension) {
        writer.writeBits(0, 9); // sps_range_extension flags
    }
    if (options.multilayer_extension) {
        writer.writeBit(false); // inter_view_mv_vert_constraint_flag
    }
    if (options.extension_3d) {
        for (unsigned d = 0; d <= 1; ++d) {
            writer.writeBits(0, 2); // iv_di_mc_enabled_flag + iv_mv_scal_enabled_flag
            if (d == 0) {
                writer.writeUE(options.ivmc_sub_pb_size_minus3);
                writer.writeBits(0, 4); // iv_res/depth_ref/vsp_mc/dbbp flags
            } else {
                writer.writeBit(false); // tex_mc_enabled_flag
                writer.writeUE(options.texmc_sub_pb_size_minus3);
                writer.writeBits(0, 5); // intra/inter prediction flags
            }
        }
    }
    if (options.scc_extension) {
        writer.writeBit(true); // sps_curr_pic_ref_enabled_flag
        if (options.truncate_scc_extension) {
            return;
        }
        writer.writeBit(options.palette_mode);
        if (options.palette_mode) {
            writer.writeUE(options.palette_max_size);
            writer.writeUE(options.delta_palette_max_predictor_size);
            writer.writeBit(options.palette_initializers);
            if (options.palette_initializers) {
                writer.writeUE(options.palette_initializer_count_minus1);
                uint32_t component_count = options.chroma_format_idc == 0 ? 1 : 3;
                for (uint32_t component = 0; component < component_count; ++component) {
                    uint32_t bit_depth = 8 + (component == 0 ? options.bit_depth_luma_minus8
                                                            : options.bit_depth_chroma_minus8);
                    for (uint32_t i = 0; i <= options.palette_initializer_count_minus1; ++i) {
                        writer.writeBits(0, bit_depth);
                    }
                }
            }
        }
        writer.writeBits(options.motion_vector_resolution_control_idc, 2);
        writer.writeBit(false); // intra_boundary_filtering_disabled_flag
    }
    if (options.extension_4bits) {
        writer.writeBits(5, 3); // future sps_extension_data_flag values
    }
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
    writeH265Tail(writer, options);
    return makeNalu({ 0x42, 0x01 }, writer.finishRbsp(options.include_rbsp_trailing_bits));
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

string makeH265VpsWithLayerSets(uint32_t count, uint32_t max_sub_layers_minus1 = 0,
                                uint32_t max_layer_id = 0, bool include_rbsp_trailing_bits = true,
                                uint32_t hrd_parameter_count = 0, bool second_common_info = false) {
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
    writer.writeBit(false); // vps_poc_proportional_to_timing_flag
    writer.writeUE(hrd_parameter_count);
    for (uint32_t i = 0; i < hrd_parameter_count; ++i) {
        writer.writeUE((std::min)(i, count)); // hrd_layer_set_idx
        bool common_info_present = i == 0 || second_common_info;
        if (i != 0) {
            writer.writeBit(common_info_present); // cprms_present_flag
        }
        if (common_info_present) {
            writer.writeBit(true); // nal_hrd_parameters_present_flag
            writer.writeBit(false); // vcl_hrd_parameters_present_flag
            writer.writeBit(false); // sub_pic_hrd_params_present_flag
            writer.writeBits(0, 8); // bit_rate_scale + cpb_size_scale
            writer.writeBits(0, 15); // HRD delay lengths
        }
        for (uint32_t sub_layer = 0; sub_layer <= max_sub_layers_minus1; ++sub_layer) {
            writer.writeBit(true); // fixed_pic_rate_general_flag
            writer.writeUE(0); // elemental_duration_in_tc_minus1
            writer.writeUE(0); // cpb_cnt_minus1
            writeH265SubLayerHrd(writer, 0, false);
        }
    }
    writer.writeBit(false); // vps_extension_flag
    return makeNalu({ 0x40, 0x01 }, writer.finishRbsp(include_rbsp_trailing_bits));
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
    for (size_t i = 0; i < compatibility_samples.size(); ++i) {
        const auto &sps = compatibility_samples[i];
        width = height = 0;
        fps = 0;
        expect(mediakit::getAVCInfo(sps, width, height, fps),
               "existing H264 SDP sample should keep parsing: " + to_string(i));
        expect(width > 0 && height > 0, "existing H264 SDP sample should produce positive dimensions");
    }

    H264SpsOptions options;
    options.vui_hrd_parameters = true;
    width = height = 0;
    fps = 0;
    expect(mediakit::getAVCInfo(makeH264Sps(options), width, height, fps),
           "complete H264 VUI and HRD syntax should parse");
    expect(width == 1280 && height == 720 && fps == 30,
           "H264 VUI timing should remain available after complete tail validation");

    options.include_rbsp_trailing_bits = false;
    width = 1920;
    height = 1080;
    fps = 25;
    expect(!mediakit::getAVCInfo(makeH264Sps(options), width, height, fps),
           "H264 SPS without rbsp_trailing_bits must fail");
    expect(width == 1920 && height == 1080 && fps == 25,
           "H264 trailing-bit failure must preserve caller outputs");

    options.include_rbsp_trailing_bits = true;
    options.hrd_cpb_cnt_minus1 = 31;
    expect(mediakit::getAVCInfo(makeH264Sps(options), width, height, fps),
           "H264 HRD CPB count upper bound should parse");
    options.hrd_cpb_cnt_minus1 = 32;
    expect(!mediakit::getAVCInfo(makeH264Sps(options), width, height, fps),
           "H264 HRD CPB count above the standard limit must fail");

    options.hrd_cpb_cnt_minus1 = 0;
    options.vcl_hrd_parameters = true;
    options.vcl_hrd_cpb_cnt_minus1 = 31;
    expect(mediakit::getAVCInfo(makeH264Sps(options), width, height, fps),
           "complete H264 VCL HRD syntax at the CPB upper bound should parse");
    options.vcl_hrd_cpb_cnt_minus1 = 32;
    expect(!mediakit::getAVCInfo(makeH264Sps(options), width, height, fps),
           "H264 VCL HRD CPB count above the standard limit must fail");
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

    string oversized_sps = validH264Sps();
    oversized_sps.resize(1024 * 1024 + 1, '\0');
    width = 1920;
    height = 1080;
    fps = 25;
    expect(!mediakit::getAVCInfo(oversized_sps, width, height, fps),
           "oversized H264 parameter sets must be rejected before RBSP allocation");
    expect(width == 1920 && height == 1080 && fps == 25,
           "oversized H264 parameter sets must preserve caller outputs");
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

    H265SpsOptions vui_options;
    vui_options.vui_hrd_parameters = true;
    width = height = 0;
    fps = 0;
    expect(mediakit::getHEVCInfo("", makeH265Sps(vui_options), width, height, fps),
           "complete H265 VUI and HRD syntax should parse");
    expect(width == 1280 && height == 720 && fps == 60,
           "H265 VUI timing should remain available after complete tail validation");

    vui_options.include_rbsp_trailing_bits = false;
    width = 1920;
    height = 1080;
    fps = 25;
    expect(!mediakit::getHEVCInfo("", makeH265Sps(vui_options), width, height, fps),
           "H265 SPS without rbsp_trailing_bits must fail");
    expect(width == 1920 && height == 1080 && fps == 25,
           "H265 trailing-bit failure must preserve caller outputs");

    vui_options.include_rbsp_trailing_bits = true;
    vui_options.hrd_cpb_cnt_minus1 = 31;
    expect(mediakit::getHEVCInfo("", makeH265Sps(vui_options), width, height, fps),
           "H265 HRD CPB count upper bound should parse");
    vui_options.hrd_cpb_cnt_minus1 = 32;
    expect(!mediakit::getHEVCInfo("", makeH265Sps(vui_options), width, height, fps),
           "H265 HRD CPB count above the standard limit must fail");

    vui_options.hrd_cpb_cnt_minus1 = 31;
    vui_options.hrd_vcl_parameters = true;
    vui_options.hrd_sub_pic_parameters = true;
    expect(mediakit::getHEVCInfo("", makeH265Sps(vui_options), width, height, fps),
           "H265 NAL/VCL sub-picture HRD syntax at the shared CPB upper bound should parse");

    vui_options.hrd_cpb_cnt_minus1 = 32;
    vui_options.hrd_fixed_pic_rate_general = false;
    vui_options.hrd_fixed_pic_rate_within_cvs = false;
    vui_options.hrd_low_delay = true;
    expect(mediakit::getHEVCInfo("", makeH265Sps(vui_options), width, height, fps),
           "H265 low-delay HRD must infer one CPB instead of consuming the absent count field");

    H265SpsOptions extension_options;
    extension_options.range_extension = true;
    extension_options.multilayer_extension = true;
    extension_options.extension_3d = true;
    extension_options.scc_extension = true;
    extension_options.extension_4bits = 1;
    expect(mediakit::getHEVCInfo("", makeH265Sps(extension_options), width, height, fps),
           "complete H265 structured SPS extensions should parse");

    extension_options = H265SpsOptions();
    extension_options.scc_extension = true;
    extension_options.truncate_scc_extension = true;
    extension_options.include_rbsp_trailing_bits = false;
    width = 1920;
    height = 1080;
    fps = 25;
    expect(!mediakit::getHEVCInfo("", makeH265Sps(extension_options), width, height, fps),
           "a required SCC extension bit must not be mistaken for rbsp_stop_one_bit");
    expect(width == 1920 && height == 1080 && fps == 25,
           "truncated H265 SPS extensions must preserve caller outputs");

    width = height = 0;
    fps = 0;
    expect(mediakit::getHEVCInfo(makeH265VpsWithLayerSets(0, 0, 0, false), makeH265Sps(H265SpsOptions()),
                                 width, height, fps),
           "an incomplete VPS must not prevent valid SPS dimensions from being extracted");
    expect(fps == 0, "VPS timing must not be published before rbsp_trailing_bits validate");

    width = height = 0;
    fps = 0;
    expect(mediakit::getHEVCInfo(makeH265VpsWithLayerSets(1, 0, 0, true, 2, false),
                                 makeH265Sps(H265SpsOptions()), width, height, fps),
           "multiple H265 VPS HRD entries should reuse common state when cprms_present_flag is false");
    expect(fps == 60, "a complete VPS with inherited HRD common state should publish timing");

    fps = 0;
    expect(mediakit::getHEVCInfo(makeH265VpsWithLayerSets(1, 0, 0, true, 2, true),
                                 makeH265Sps(H265SpsOptions()), width, height, fps),
           "multiple H265 VPS HRD entries with repeated common state should parse");
    expect(fps == 60, "a complete VPS with repeated HRD common state should publish timing");

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
    options.log2_min_luma_coding_block_size_minus3 = 1;
    expect(parses(options), "H265 minimum coding-block log2 value adjacent to the lower bound should parse");
    options.log2_min_luma_coding_block_size_minus3 = 3;
    expect(parses(options), "H265 minimum coding-block log2 upper bound should parse");
    options.log2_min_luma_coding_block_size_minus3 = 4;
    expect(!parses(options), "H265 minimum coding-block log2 value above the standard limit must fail");

    options = H265SpsOptions();
    options.log2_diff_max_min_luma_coding_block_size = 1;
    expect(parses(options), "H265 coding-block log2 difference adjacent to the lower bound should parse");
    options.log2_diff_max_min_luma_coding_block_size = 3;
    expect(parses(options), "H265 coding-block log2 difference upper bound should parse");
    options.log2_diff_max_min_luma_coding_block_size = 4;
    expect(!parses(options), "H265 coding-block log2 difference above the standard limit must fail");

    options = H265SpsOptions();
    options.log2_min_luma_coding_block_size_minus3 = 1;
    options.log2_diff_max_min_luma_coding_block_size = 1;
    options.extension_3d = true;
    options.ivmc_sub_pb_size_minus3 = 1;
    options.texmc_sub_pb_size_minus3 = 1;
    expect(parses(options), "H265 3D sub-block sizes at their derived lower bound should parse");
    options.ivmc_sub_pb_size_minus3 = 2;
    options.texmc_sub_pb_size_minus3 = 2;
    expect(parses(options), "H265 3D sub-block sizes at their derived upper bound should parse");
    options.ivmc_sub_pb_size_minus3 = 0;
    expect(!parses(options), "H265 3D sub-block size below its derived lower bound must fail");
    options.ivmc_sub_pb_size_minus3 = 1;
    options.texmc_sub_pb_size_minus3 = 3;
    expect(!parses(options), "H265 3D sub-block size above its derived upper bound must fail");

    options = H265SpsOptions();
    options.scc_extension = true;
    options.palette_mode = true;
    options.palette_max_size = 64;
    options.delta_palette_max_predictor_size = 64;
    options.palette_initializers = true;
    options.palette_initializer_count_minus1 = 127;
    options.motion_vector_resolution_control_idc = 2;
    expect(parses(options), "H265 SCC palette and initializer upper bounds should parse");
    options.palette_max_size = 65;
    expect(!parses(options), "H265 SCC palette size above the profile limit must fail");
    options.palette_max_size = 64;
    options.delta_palette_max_predictor_size = 65;
    expect(!parses(options), "H265 SCC derived predictor size above the profile limit must fail");
    options.delta_palette_max_predictor_size = 64;
    options.palette_initializer_count_minus1 = 128;
    expect(!parses(options), "H265 SCC initializer count above the standard storage limit must fail");
    options.palette_initializer_count_minus1 = 127;
    options.motion_vector_resolution_control_idc = 3;
    expect(!parses(options), "reserved H265 motion-vector resolution control must fail");

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

    string oversized_sps = validH265Sps();
    oversized_sps.resize(1024 * 1024 + 1, '\0');
    width = 1920;
    height = 1080;
    fps = 25;
    expect(!mediakit::getHEVCInfo("", oversized_sps, width, height, fps),
           "oversized H265 SPS parameter sets must be rejected before RBSP allocation");
    expect(width == 1920 && height == 1080 && fps == 25,
           "oversized H265 SPS parameter sets must preserve caller outputs");

    string oversized_vps = validH265Vps();
    oversized_vps.resize(1024 * 1024 + 1, '\0');
    width = height = 0;
    fps = 0;
    expect(mediakit::getHEVCInfo(oversized_vps, makeH265Sps(H265SpsOptions()), width, height, fps),
           "an oversized VPS must not prevent valid SPS dimensions from being extracted");
    expect(fps == 0, "oversized VPS data must not publish timing");
}

class CountingH264Track : public mediakit::H264Track {
public:
    bool update() override {
        ++update_count;
        return H264Track::update();
    }

    unsigned update_count = 0;
};

class CountingH265Track : public mediakit::H265Track {
public:
    bool update() override {
        ++update_count;
        return H265Track::update();
    }

    unsigned update_count = 0;
};

void testTrackParserRetry() {
    H264SpsOptions h264_options;
    h264_options.include_rbsp_trailing_bits = false;
    CountingH264Track h264_track;
    h264_track.inputFrame(mediakit::createConfigFrame<mediakit::H264Frame>(makeH264Sps(h264_options), 0, 0));
    h264_track.inputFrame(mediakit::createConfigFrame<mediakit::H264Frame>(string("\x68\x80", 2), 0, 0));
    expect(h264_track.update_count == 1, "H264 track should parse once when its initial configuration becomes ready");
    h264_track.inputFrame(mediakit::createConfigFrame<mediakit::H264Frame>(string("\x68\x80", 2), 0, 0));
    expect(h264_track.update_count == 1, "repeated H264 PPS frames must not reparse an unchanged invalid SPS");
    h264_track.inputFrame(mediakit::createConfigFrame<mediakit::H264Frame>(string("\x65\x80", 2), 0, 0));
    expect(h264_track.update_count == 1, "ordinary H264 frames must not retry an unchanged invalid SPS");
    h264_track.inputFrame(mediakit::createConfigFrame<mediakit::H264Frame>(makeH264Sps(H264SpsOptions()), 0, 0));
    expect(h264_track.update_count == 2 && h264_track.getVideoWidth() == 1280,
           "a replacement H264 SPS should retry parsing and publish dimensions");

    H265SpsOptions h265_options;
    h265_options.include_rbsp_trailing_bits = false;
    CountingH265Track h265_track;
    h265_track.inputFrame(mediakit::createConfigFrame<mediakit::H265Frame>(makeH265VpsWithLayerSets(0), 0, 0));
    h265_track.inputFrame(mediakit::createConfigFrame<mediakit::H265Frame>(makeH265Sps(h265_options), 0, 0));
    h265_track.inputFrame(mediakit::createConfigFrame<mediakit::H265Frame>(string("\x44\x01\x80", 3), 0, 0));
    expect(h265_track.update_count == 1, "H265 track should parse once when its initial configuration becomes ready");
    h265_track.inputFrame(mediakit::createConfigFrame<mediakit::H265Frame>(string("\x44\x01\x80", 3), 0, 0));
    expect(h265_track.update_count == 1, "repeated H265 PPS frames must not reparse an unchanged invalid SPS");
    h265_track.inputFrame(mediakit::createConfigFrame<mediakit::H265Frame>(makeH265VpsWithLayerSets(0), 0, 0));
    expect(h265_track.update_count == 1, "repeated H265 VPS frames must not reparse an unchanged invalid SPS");
    h265_track.inputFrame(mediakit::createConfigFrame<mediakit::H265Frame>(string("\x26\x01\x80", 3), 0, 0));
    expect(h265_track.update_count == 1, "ordinary H265 frames must not retry an unchanged invalid SPS");
    h265_track.inputFrame(mediakit::createConfigFrame<mediakit::H265Frame>(makeH265Sps(H265SpsOptions()), 0, 0));
    expect(h265_track.update_count == 2 && h265_track.getVideoWidth() == 1280,
           "a replacement H265 SPS should retry parsing and publish dimensions");
}

void testH264TrackExtraDataCapacity() {
#ifdef ENABLE_MP4
    mpeg4_avc_t avc = {};
    string pps("\x68\x80", 2);
    string sps = validH264Sps();
    sps.resize(sizeof(avc.data) - pps.size(), '\xff');

    mediakit::H264Track track(sps, pps, 0, 0);
    auto extra_data = track.getExtraData();
    expect(extra_data && extra_data->size() > 0,
           "H264 parameter sets that exactly fit the AVC configuration storage must serialize");
    mpeg4_avc_t decoded_avc = {};
    expect(mpeg4_avc_decoder_configuration_record_load((const uint8_t *)extra_data->data(), extra_data->size(), &decoded_avc) > 0 &&
               decoded_avc.off == sizeof(decoded_avc.data),
           "the serialized AVC configuration must preserve all parameter-set bytes");

    sps.push_back('\xff');
    mediakit::H264Track oversized_track(sps, pps, 0, 0);
    expect(oversized_track.getExtraData() == nullptr,
           "H264 parameter sets larger than the AVC configuration storage must fail without entering the converter");
#else
    string pps("\x68\x80", 2);
    string sps = validH264Sps();
    sps.resize(UINT16_MAX, '\xff');

    mediakit::H264Track track(sps, pps, 0, 0);
    auto extra_data = track.getExtraData();
    expect(extra_data && extra_data->size() > sps.size(),
           "the largest H264 SPS representable by an AVC configuration record must serialize");

    sps.push_back('\xff');
    mediakit::H264Track oversized_track(sps, pps, 0, 0);
    expect(oversized_track.getExtraData() == nullptr,
           "an H264 SPS larger than the 16-bit AVC configuration record field must be rejected");
#endif
}

void testH265TrackExtraDataCapacity() {
#ifdef ENABLE_MP4
    mpeg4_hevc_t hevc = {};
    string vps = validH265Vps();
    string pps("\x44\x01\x80", 3);
    string sps = validH265Sps();
    sps.resize(sizeof(hevc.data) - vps.size() - pps.size(), '\xff');

    mediakit::H265Track track(vps, sps, pps, 0, 0, 0);
    auto extra_data = track.getExtraData();
    expect(extra_data && extra_data->size() > 0,
           "H265 parameter sets that exactly fit the HEVC configuration storage must serialize");
    mpeg4_hevc_t decoded_hevc = {};
    expect(mpeg4_hevc_decoder_configuration_record_load((const uint8_t *)extra_data->data(), extra_data->size(), &decoded_hevc) > 0 &&
               decoded_hevc.off == sizeof(decoded_hevc.data),
           "the serialized HEVC configuration must preserve all parameter-set bytes");

    sps.push_back('\xff');
    mediakit::H265Track oversized_track(vps, sps, pps, 0, 0, 0);
    expect(oversized_track.getExtraData() == nullptr,
           "H265 parameter sets larger than the HEVC configuration storage must fail without entering the converter");
#endif
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
        if (group == "all" || group == "track-retry") {
            testTrackParserRetry();
        }
        if (group == "all" || group == "h264-extra-data") {
            testH264TrackExtraDataCapacity();
        }
        if (group == "all" || group == "h265-extra-data") {
            testH265TrackExtraDataCapacity();
        }
        expect(group == "all" || group == "h264-basic" || group == "h264-limits" || group == "h265" ||
                   group == "track-retry" || group == "h264-extra-data" || group == "h265-extra-data",
               "unknown test group");
        cout << "test_sps_parser passed" << endl;
        return 0;
    } catch (const exception &ex) {
        cerr << "test_sps_parser failed: " << ex.what() << endl;
        return EXIT_FAILURE;
    }
}
