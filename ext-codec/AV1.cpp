/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "AV1.h"
#include "AV1Rtp.h"
#include "VpxRtmp.h"
#include "BitStream.h"
#include "Extension/Factory.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

class AV1BitStreamReader : public BitStreamReader {
public:
    uint64_t get_leb128(size_t &consumed) {
        if (_bit_index % 8) {
            throw std::invalid_argument("Bad bit index");
        }
        auto ret = AV1Track::leb128(_buffer + (_bit_index >> 3), (_bit_size - _bit_index) >> 3, consumed);
        _bit_index += (consumed << 3);
        return ret;
    }

    // 4.10.3. uvlc()
    uint32_t get_uvlc() {
        uint32_t leadingZeros = 0;
        while (true) {
            auto done = get_bits(1);
            if (done) {
                break;
            }
            ++leadingZeros;
        }
        if (leadingZeros >= 32) {
            return UINT32_MAX; //((1 << 32) - 1);
        }
        auto value = get_bits(leadingZeros);
        return value + (1 << leadingZeros) - 1;
    }
};

enum class AV1FrameType {
    Reserved = 0,
    OBU_SEQUENCE_HEADER,
    OBU_TEMPORAL_DELIMITER,
    OBU_FRAME_HEADER,
    OBU_TILE_GROUP,
    OBU_METADATA,
    OBU_FRAME,
    OBU_REDUNDANT_FRAME_HEADER,
    OBU_TILE_LIST,
    // 9-14 Reserved
    OBU_PADDING = 15
};

#pragma pack(push, 1)

class AV1_ObuHeader {
public:
#if __BYTE_ORDER == __BIG_ENDIAN
    unsigned obu_forbidden_bit : 1;
    unsigned obu_type : 4;
    unsigned obu_extension_flag : 1;
    unsigned obu_has_size_field : 1;
    unsigned obu_reserved_1bit : 1;
#else
    unsigned obu_reserved_1bit : 1;
    unsigned obu_has_size_field : 1;
    unsigned obu_extension_flag : 1;
    unsigned obu_type : 4;
    unsigned obu_forbidden_bit : 1;
#endif
};

// https://aomediacodec.github.io/av1-isobmff/#av1codecconfigurationbox-syntax
struct AV1CodecConfigurationRecord {
    static constexpr auto kOffset = 4u;
#if __BYTE_ORDER == __BIG_ENDIAN
    uint32_t marker : 1; // = 1
    uint32_t version : 7; // = 1
#else
    uint32_t version : 7; // = 1
    uint32_t marker : 1; // = 1
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
    uint32_t seq_profile : 3;
    uint32_t seq_level_idx_0 : 5;
#else
    uint32_t seq_level_idx_0 : 5;
    uint32_t seq_profile : 3;
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
    uint32_t seq_tier_0 : 1;
    uint32_t high_bitdepth : 1;
    uint32_t twelve_bit : 1;
    uint32_t monochrome : 1;
    uint32_t chroma_subsampling_x : 1;
    uint32_t chroma_subsampling_y : 1;
    uint32_t chroma_sample_position : 2;
#else
    uint32_t chroma_sample_position : 2;
    uint32_t chroma_subsampling_y : 1;
    uint32_t chroma_subsampling_x : 1;
    uint32_t monochrome : 1;
    uint32_t twelve_bit : 1;
    uint32_t high_bitdepth : 1;
    uint32_t seq_tier_0 : 1;
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
    uint32_t reserved : 3; // = 0
    uint32_t initial_presentation_delay_present : 1;
    uint32_t initial_presentation_delay_minus_one : 4;
#else
    uint32_t initial_presentation_delay_minus_one : 4;
    uint32_t initial_presentation_delay_present : 1;
    uint32_t reserved : 3; // = 0
#endif
    uint8_t configOBUs[1];
};

#pragma pack(pop)

#define NUM_REF_FRAMES 8
#define KEY_FRAME 0
#define INTER_FRAME 1
#define INTRA_ONLY_FRAME 2
#define SWITCH_FRAME 2

struct AV1FrameStateStorage {
    size_t width = 0;
    size_t height = 0;
    float fps = 0;
    int OperatingPointIdc = 0;
    int SeenFrameHeader = 0;
    int TileNum = 0;
    int BitDepth;
    int NumPlanes;
    int OrderHintBits;
    bool FrameIsIntra;
    uint8_t RefFrameType[8];

    struct {
        uint8_t seq_profile; // f(3)
        bool still_picture; // f(1)
        bool reduced_still_picture_header; // f(1)
        uint8_t seq_level_idx[32];
        bool timing_info_present_flag;
        bool decoder_model_info_present_flag;
        bool initial_display_delay_present_flag;
        uint8_t operating_points_cnt_minus_1;
        uint16_t operating_point_idc[32];
        bool seq_tier[32];
        bool decoder_model_present_for_this_op[32];
        bool initial_display_delay_present_for_this_op[32];
        uint8_t initial_display_delay_minus_1[32];
        uint8_t frame_width_bits_minus_1;
        uint8_t frame_height_bits_minus_1;
        uint32_t max_frame_width_minus_1;
        uint32_t max_frame_height_minus_1;
        bool frame_id_numbers_present_flag;
        uint8_t delta_frame_id_length_minus_2;
        uint8_t additional_frame_id_length_minus_1;
        bool use_128x128_superblock;
        bool enable_filter_intra;
        bool enable_intra_edge_filter;
        bool enable_interintra_compound;

        bool enable_masked_compound;
        bool enable_warped_motion;
        bool enable_dual_filter;
        bool enable_order_hint;

        bool enable_jnt_comp;
        bool enable_ref_frame_mvs;

        bool seq_choose_screen_content_tools;
        bool seq_force_screen_content_tools;

        bool seq_choose_integer_mv;
        bool seq_force_integer_mv;
        bool eq_force_integer_mv;

        uint8_t order_hint_bits_minus_1;
        bool enable_superres;
        bool enable_cdef;
        bool enable_restoration;
        bool film_grain_params_present;

        struct {
            uint32_t num_units_in_display_tick;
            uint32_t time_scale;
            bool equal_picture_interval;
            int num_ticks_per_picture_minus_1;
        } timing_info;

        struct {
            uint8_t buffer_delay_length_minus_1;
            uint32_t num_units_in_decoding_tick;
            uint8_t buffer_removal_time_length_minus_1;
            uint8_t frame_presentation_time_length_minus_1;
        } decoder_model_info;

        struct {
            uint32_t decoder_buffer_delay[32];
            uint32_t encoder_buffer_delay[32];
            bool low_delay_mode_flag[32];
        } operating_parameters_info;

        struct {
          bool high_bitdepth;
          bool twelve_bit;
          bool mono_chrome;
          bool color_description_present_flag;
          uint8_t color_primaries;
          uint8_t transfer_characteristics;
          uint8_t matrix_coefficients;
          bool color_range;
          bool subsampling_x;
          bool subsampling_y;
          uint8_t chroma_sample_position;
          bool separate_uv_delta_q;
        } color_config;
    } sequence_header_obu;
};

// https://aomediacodec.github.io/av1-spec/av1-spec.pdf
struct AV1FrameOBUContext : public AV1FrameStateStorage {
    AV1BitStreamReader reader;
    size_t obu_size;

    struct {
        bool obu_forbidden_bit; // f(1)
        AV1FrameType obu_type; // f(4)
        bool obu_extension_flag; // f(1)
        bool obu_has_size_field; // f(1)
        bool obu_reserved_1bit; // f(1)
    } obu_header;

    struct {
        uint8_t temporal_id; // f(3)
        uint8_t spatial_id; // f(2)
        uint8_t extension_header_reserved_3bits; // f(3)
    } obu_extension_header;

    union {
        struct {
            bool show_existing_frame;
            uint8_t frame_to_show_map_idx;
            uint32_t display_frame_id;
            uint8_t frame_type;
            bool show_frame;
            bool showable_frame;
            bool error_resilient_mode;
        } uncompressed_header;
    };

    void load(const void *data, size_t size);
    void drop_obu() {}
    void load_obu_header();
    void load_obu_extension_header();
    void load_sequence_header_obu();
    void load_temporal_delimiter_obu();
    void load_frame_header_obu();
    void load_tile_group_obu(size_t) {}
    void load_metadata_obu() {}
    void load_frame_obu(size_t sz);
    void load_tile_list_obu() {}
    void load_padding_obu() {}
    void load_color_config();
    void load_timing_info();
    void load_decoder_model_info();
    void load_operating_parameters_info(int op);
    void load_choose_operating_point();
    void load_uncompressed_header();
    void load_temporal_point_info() {}
    void load_grain_params(uint8_t idx) {}

    // 5.4. Reserved OBU syntax
    void load_reserved_obu() {}

    void load_trailing_bits(size_t bits);
    void load_byte_alignment();
};

// 5.3.1. General OBU syntax
void AV1FrameOBUContext::load(const void *data, size_t size) {
    reader.setup(data, size);
    load_obu_header();
    if (obu_header.obu_extension_flag) {
        load_obu_extension_header();
    }
    if (obu_header.obu_has_size_field) {
        size_t consumed;
        obu_size = reader.get_leb128(consumed);
        CHECK(obu_size + 1 + obu_header.obu_extension_flag + consumed == size);
    } else {
        obu_size = size - 1 - obu_header.obu_extension_flag;
    }
    auto startPosition = reader.get_bit_index();
    if (obu_header.obu_type != AV1FrameType::OBU_SEQUENCE_HEADER
        && obu_header.obu_type != AV1FrameType::OBU_TEMPORAL_DELIMITER && OperatingPointIdc != 0
        && obu_header.obu_extension_flag == 1) {
        auto inTemporalLayer = (OperatingPointIdc >> obu_extension_header.temporal_id) & 1;
        auto inSpatialLayer = (OperatingPointIdc >> (obu_extension_header.spatial_id + 8)) & 1;
        if (!inTemporalLayer || !inSpatialLayer) {
            drop_obu();
            return;
        }
    }
    switch (obu_header.obu_type) {
        case AV1FrameType::OBU_SEQUENCE_HEADER: load_sequence_header_obu(); break;
        case AV1FrameType::OBU_TEMPORAL_DELIMITER: load_temporal_delimiter_obu(); break;
        case AV1FrameType::OBU_FRAME_HEADER: SeenFrameHeader = 0; load_frame_header_obu(); break;
        case AV1FrameType::OBU_REDUNDANT_FRAME_HEADER: SeenFrameHeader = 1; load_frame_header_obu(); break;
        case AV1FrameType::OBU_TILE_GROUP: load_tile_group_obu(obu_size); break;
        case AV1FrameType::OBU_METADATA: load_metadata_obu(); break;
        case AV1FrameType::OBU_FRAME: load_frame_obu(obu_size); break;
        case AV1FrameType::OBU_TILE_LIST: load_tile_list_obu(); break;
        case AV1FrameType::OBU_PADDING: load_padding_obu(); break;
        default: load_reserved_obu(); break;
    }

#if 0
    auto currentPosition = reader.get_bit_index();
    auto payloadBits = currentPosition - startPosition;
    if (obu_size > 0 && obu_header.obu_type != AV1FrameType::OBU_TILE_GROUP
        && obu_header.obu_type != AV1FrameType::OBU_TILE_LIST && obu_header.obu_type != AV1FrameType::OBU_FRAME) {
        load_trailing_bits(obu_size * 8 - payloadBits);
    }
#endif
}

// 5.3.2. OBU header syntax
void AV1FrameOBUContext::load_obu_header() {
    obu_header.obu_forbidden_bit = reader.get_bits(1);
    obu_header.obu_type = (AV1FrameType)reader.get_bits(4);
    obu_header.obu_extension_flag = reader.get_bits(1);
    obu_header.obu_has_size_field = reader.get_bits(1);
    obu_header.obu_reserved_1bit = reader.get_bits(1);
}

// 5.3.3. OBU extension header syntax
void AV1FrameOBUContext::load_obu_extension_header() {
    obu_extension_header.temporal_id = reader.get_bits(3);
    obu_extension_header.spatial_id = reader.get_bits(2);
    obu_extension_header.extension_header_reserved_3bits = reader.get_bits(3);
}

// 5.3.4. Trailing bits syntax
void AV1FrameOBUContext::load_trailing_bits(size_t bits) {
    auto trailing_one_bit = reader.get_bits(1);
    bits--;
    while (bits > 0) {
        auto trailing_zero_bit = reader.get_bits(1);
        bits--;
    }
}

// 5.3.5. Byte alignment syntax
void AV1FrameOBUContext::load_byte_alignment() {
    while (reader.get_bit_index() & 7) {
        auto zero_bit = reader.get_bits(1);
    }
}

// 5.5.3. Timing info syntax
void AV1FrameOBUContext::load_timing_info() {
    sequence_header_obu.timing_info.num_units_in_display_tick = reader.get_bits(32);
    sequence_header_obu.timing_info.time_scale = reader.get_bits(32);
    sequence_header_obu.timing_info.equal_picture_interval = reader.get_bits(1);
    if (sequence_header_obu.timing_info.equal_picture_interval) {
        sequence_header_obu.timing_info.num_ticks_per_picture_minus_1 = reader.get_uvlc();
    }
}

// 5.5.4. Decoder model info syntax
void AV1FrameOBUContext::load_decoder_model_info() {
    sequence_header_obu.decoder_model_info.buffer_delay_length_minus_1 = reader.get_bits(5);
    sequence_header_obu.decoder_model_info.num_units_in_decoding_tick = reader.get_bits(32);
    sequence_header_obu.decoder_model_info.buffer_removal_time_length_minus_1 = reader.get_bits(5);
    sequence_header_obu.decoder_model_info.frame_presentation_time_length_minus_1 = reader.get_bits(5);
}

// 5.5.5. Operating parameters info syntax
void AV1FrameOBUContext::load_operating_parameters_info(int op) {
    auto n = sequence_header_obu.decoder_model_info.buffer_delay_length_minus_1 + 1;
    sequence_header_obu.operating_parameters_info.decoder_buffer_delay[op] = reader.get_bits(n);
    sequence_header_obu.operating_parameters_info.encoder_buffer_delay[op] = reader.get_bits(n);
    sequence_header_obu.operating_parameters_info.low_delay_mode_flag[op] = reader.get_bits(1);
}

#define SELECT_INTEGER_MV 2
#define SELECT_SCREEN_CONTENT_TOOLS 2

// 5.5.1. General sequence header OBU syntax
void AV1FrameOBUContext::load_sequence_header_obu() {
    sequence_header_obu.seq_profile = reader.get_bits(3);
    sequence_header_obu.still_picture = reader.get_bits(1);
    sequence_header_obu.reduced_still_picture_header = reader.get_bits(1);
    if (sequence_header_obu.reduced_still_picture_header) {
        sequence_header_obu.timing_info_present_flag = 0;
        sequence_header_obu.decoder_model_info_present_flag = 0;
        sequence_header_obu.initial_display_delay_present_flag = 0;
        sequence_header_obu.operating_points_cnt_minus_1 = 0;
        sequence_header_obu.operating_point_idc[0] = 0;
        sequence_header_obu.seq_level_idx[0] = reader.get_bits(5);
        sequence_header_obu.seq_tier[0] = 0;
        sequence_header_obu.decoder_model_present_for_this_op[0] = 0;
        sequence_header_obu.initial_display_delay_present_for_this_op[0] = 0;
    } else {
        sequence_header_obu.timing_info_present_flag = reader.get_bits(1);
        if (sequence_header_obu.timing_info_present_flag) {
            load_timing_info();
            sequence_header_obu.decoder_model_info_present_flag = reader.get_bits(1);
            if (sequence_header_obu.decoder_model_info_present_flag) {
                load_decoder_model_info();
            }
        } else {
            sequence_header_obu.decoder_model_info_present_flag = 0;
        }
        sequence_header_obu.initial_display_delay_present_flag = reader.get_bits(1);
        sequence_header_obu.operating_points_cnt_minus_1 = reader.get_bits(5);
        for (size_t i = 0; i <= sequence_header_obu.operating_points_cnt_minus_1; ++i) {
            sequence_header_obu.operating_point_idc[i] = reader.get_bits(12);
            sequence_header_obu.seq_level_idx[i] = reader.get_bits(5);
            if (sequence_header_obu.seq_level_idx[i] > 7) {
                sequence_header_obu.seq_tier[i] = reader.get_bits(1);
            } else {
                sequence_header_obu.seq_tier[i] = 0;
            }
            if (sequence_header_obu.decoder_model_info_present_flag) {
                sequence_header_obu.decoder_model_present_for_this_op[i] = reader.get_bits(1);
                if (sequence_header_obu.decoder_model_present_for_this_op[i]) {
                    load_operating_parameters_info(i);
                }
            } else {
                sequence_header_obu.decoder_model_present_for_this_op[i] = 0;
            }
            if (sequence_header_obu.initial_display_delay_present_flag) {
                sequence_header_obu.initial_display_delay_present_for_this_op[i] = reader.get_bits(1);
                if (sequence_header_obu.initial_display_delay_present_for_this_op[i]) {
                    sequence_header_obu.initial_display_delay_minus_1[i] = reader.get_bits(4);
                }
            }
        }
    }
    auto operatingPoint = sequence_header_obu.operating_points_cnt_minus_1;// 0~ operating_points_cnt_minus_1 ;choose_operating_point();
    OperatingPointIdc = sequence_header_obu.operating_point_idc[operatingPoint];
    sequence_header_obu.frame_width_bits_minus_1 = reader.get_bits(4);
    sequence_header_obu.frame_height_bits_minus_1 = reader.get_bits(4);
    auto n = sequence_header_obu.frame_width_bits_minus_1 + 1;
    sequence_header_obu.max_frame_width_minus_1 = reader.get_bits(n);
    n = sequence_header_obu.frame_height_bits_minus_1 + 1;
    sequence_header_obu.max_frame_height_minus_1 = reader.get_bits(n);

    width = sequence_header_obu.max_frame_width_minus_1 + 1;
    height = sequence_header_obu.max_frame_height_minus_1 + 1;
    if (sequence_header_obu.reduced_still_picture_header) {
        sequence_header_obu.frame_id_numbers_present_flag = 0;
    } else {
        sequence_header_obu.frame_id_numbers_present_flag = reader.get_bits(1);
    }

    if (sequence_header_obu.frame_id_numbers_present_flag) {
        sequence_header_obu.delta_frame_id_length_minus_2 = reader.get_bits(4);
        sequence_header_obu.additional_frame_id_length_minus_1 = reader.get_bits(3);
    }
    sequence_header_obu.use_128x128_superblock = reader.get_bits(1);
    sequence_header_obu.enable_filter_intra = reader.get_bits(1);
    sequence_header_obu.enable_intra_edge_filter = reader.get_bits(1);
    if (sequence_header_obu.reduced_still_picture_header) {
        sequence_header_obu.enable_interintra_compound = 0;
        sequence_header_obu.enable_masked_compound = 0;
        sequence_header_obu.enable_warped_motion = 0;
        sequence_header_obu.enable_dual_filter = 0;
        sequence_header_obu.enable_order_hint = 0;
        sequence_header_obu.enable_jnt_comp = 0;
        sequence_header_obu.enable_ref_frame_mvs = 0;
        sequence_header_obu.seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
        sequence_header_obu.seq_force_integer_mv = SELECT_INTEGER_MV;
        OrderHintBits = 0;
    } else {
        sequence_header_obu.enable_interintra_compound = reader.get_bits(1);
        sequence_header_obu.enable_masked_compound = reader.get_bits(1);
        sequence_header_obu.enable_warped_motion = reader.get_bits(1);
        sequence_header_obu.enable_dual_filter = reader.get_bits(1);
        sequence_header_obu.enable_order_hint = reader.get_bits(1);
        if (sequence_header_obu.enable_order_hint) {
            sequence_header_obu.enable_jnt_comp = reader.get_bits(1);
            sequence_header_obu.enable_ref_frame_mvs = reader.get_bits(1);
        } else {
            sequence_header_obu.enable_jnt_comp = 0;
            sequence_header_obu.enable_ref_frame_mvs = 0;
        }
        sequence_header_obu.seq_choose_screen_content_tools = reader.get_bits(1);
        if (sequence_header_obu.seq_choose_screen_content_tools) {
            sequence_header_obu.seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
        } else {
            sequence_header_obu.seq_force_screen_content_tools = reader.get_bits(1);
        }
        if (sequence_header_obu.seq_force_screen_content_tools > 0) {
            sequence_header_obu.seq_choose_integer_mv = reader.get_bits(1);
            if (sequence_header_obu.seq_choose_integer_mv) {
                sequence_header_obu.seq_force_integer_mv = SELECT_INTEGER_MV;
            } else {
                sequence_header_obu.eq_force_integer_mv = reader.get_bits(1);
            }
        } else {
            sequence_header_obu.seq_force_integer_mv = SELECT_INTEGER_MV;
        }
        if (sequence_header_obu.enable_order_hint) {
            sequence_header_obu.order_hint_bits_minus_1 = reader.get_bits(3);
            OrderHintBits = sequence_header_obu.order_hint_bits_minus_1 + 1;
        } else {
            OrderHintBits = 0;
        }
    }
    sequence_header_obu.enable_superres = reader.get_bits(1);
    sequence_header_obu.enable_cdef = reader.get_bits(1);
    sequence_header_obu.enable_restoration = reader.get_bits(1);
    load_color_config();
    sequence_header_obu.film_grain_params_present = reader.get_bits(1);
}

#define CP_BT_709 1
#define CP_UNSPECIFIED 2

#define TC_UNSPECIFIED 2
#define TC_SRGB 13

#define MC_IDENTITY 0
#define MC_UNSPECIFIED 2

#define CSP_UNKNOWN 0

// 5.5.2. Color config syntax
void AV1FrameOBUContext::load_color_config() {
    auto &thiz = sequence_header_obu.color_config;
    thiz.high_bitdepth = reader.get_bits(1);
    if (sequence_header_obu.seq_profile == 2 && thiz.high_bitdepth) {
        thiz.twelve_bit = reader.get_bits(1);
        BitDepth = thiz.twelve_bit ? 12 : 10;
    } else if (sequence_header_obu.seq_profile <= 2) {
        BitDepth = thiz.high_bitdepth ? 10 : 8;
    }
    if (sequence_header_obu.seq_profile == 1) {
        thiz.mono_chrome = 0;
    } else {
        thiz.mono_chrome = reader.get_bits(1);
    }
    NumPlanes = thiz.mono_chrome ? 1 : 3;
    thiz.color_description_present_flag = reader.get_bits(1);

    if (thiz.color_description_present_flag) {
        thiz.color_primaries = reader.get_bits(8);
        thiz.transfer_characteristics = reader.get_bits(8);
        thiz.matrix_coefficients = reader.get_bits(8);
    } else {
        thiz.color_primaries = CP_UNSPECIFIED;
        thiz.transfer_characteristics = TC_UNSPECIFIED;
        thiz.matrix_coefficients = MC_UNSPECIFIED;
    }

    if (thiz.mono_chrome) {
        thiz.color_range = reader.get_bits(1);
        thiz.subsampling_x = 1;
        thiz.subsampling_y = 1;
        thiz.chroma_sample_position = CSP_UNKNOWN;
        thiz.separate_uv_delta_q = 0;
        return;
    }
    if (thiz.color_primaries == CP_BT_709 &&
        thiz.transfer_characteristics == TC_SRGB &&
        thiz.matrix_coefficients == MC_IDENTITY) {
        thiz.color_range = 1;
        thiz.subsampling_x = 0;
        thiz.subsampling_y = 0;
    } else {
        thiz.color_range = reader.get_bits(1);
        if (sequence_header_obu.seq_profile == 0) {
            thiz.subsampling_x = 1;
            thiz.subsampling_y = 1;
        } else if (sequence_header_obu.seq_profile == 1) {
            thiz.subsampling_x = 0;
            thiz.subsampling_y = 0;
        } else {
            if (BitDepth == 12) {
                thiz.subsampling_x = reader.get_bits(1);
                if (thiz.subsampling_x) {
                    thiz.subsampling_y = reader.get_bits(1);
                } else {
                    thiz.subsampling_y = 0;
                }
            } else {
                thiz.subsampling_x = 1;
                thiz.subsampling_y = 0;
            }
        }
        if (thiz.subsampling_x && thiz.subsampling_y) {
            thiz.chroma_sample_position = reader.get_bits(2);
        }
    }
    thiz.separate_uv_delta_q = reader.get_bits(1);
}

void AV1FrameOBUContext::load_temporal_delimiter_obu() {
    SeenFrameHeader = 0;
}

void AV1FrameOBUContext::load_frame_obu(size_t sz) {
    auto startBitPos = reader.get_bit_index();
    load_frame_header_obu();
    load_byte_alignment();
    auto endBitPos = reader.get_bit_index();
    auto headerBytes = (endBitPos - startBitPos) / 8;
    load_tile_group_obu(sz - headerBytes);
}

void AV1FrameOBUContext::load_frame_header_obu() {
    if (SeenFrameHeader == 1) {
        // frame_header_copy();
    } else {
        SeenFrameHeader = 1;
        load_uncompressed_header();
        if (uncompressed_header.show_existing_frame) {
            // decode_frame_wrapup();
            SeenFrameHeader = 0;
        } else {
            TileNum = 0;
            SeenFrameHeader = 1;
        }
    }
}

void AV1FrameOBUContext::load_uncompressed_header() {
    size_t idLen;
    if (sequence_header_obu.frame_id_numbers_present_flag) {
        idLen = sequence_header_obu.additional_frame_id_length_minus_1 + sequence_header_obu.delta_frame_id_length_minus_2 + 3 ;
    }
    auto allFrames = (1 << NUM_REF_FRAMES) - 1;
    if (sequence_header_obu.reduced_still_picture_header) {
        uncompressed_header.show_existing_frame = 0;
        uncompressed_header.frame_type = KEY_FRAME;
        FrameIsIntra = 1;
        uncompressed_header.show_frame = 1;
        uncompressed_header.showable_frame = 0;
    } else {
        uncompressed_header.show_existing_frame = reader.get_bits(1);
        if (uncompressed_header.show_existing_frame == 1) {
            uncompressed_header.frame_to_show_map_idx = reader.get_bits(3);
            if (sequence_header_obu.decoder_model_info_present_flag
                && !sequence_header_obu.timing_info.equal_picture_interval) {
                load_temporal_point_info();
            }
            auto refresh_frame_flags = 0;
            if (sequence_header_obu.frame_id_numbers_present_flag) {
                uncompressed_header.display_frame_id = reader.get_bits(idLen);
            }
            uncompressed_header.frame_type = RefFrameType[uncompressed_header.frame_to_show_map_idx];
            if ( uncompressed_header.frame_type == KEY_FRAME ) {
                refresh_frame_flags = allFrames;
            }
            if (sequence_header_obu.film_grain_params_present) {
                load_grain_params(uncompressed_header.frame_to_show_map_idx);
            }
            return;
        }
        uncompressed_header.frame_type = reader.get_bits(2);
        FrameIsIntra = (uncompressed_header.frame_type == INTRA_ONLY_FRAME || uncompressed_header.frame_type == KEY_FRAME);
        uncompressed_header.show_frame = reader.get_bits(1);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////

uint64_t AV1Track::leb128(const uint8_t *ptr, size_t len, size_t &consumed) {
    uint64_t value = 0;
    consumed = 0;
    for (size_t i = 0; i < 8; ++i) {
        CHECK(consumed < len, "leb128 not enough buf");
        auto &leb128_byte = ptr[i];
        value |= ((leb128_byte & 0x7f) << (i * 7));
        consumed += 1;
        if (!(leb128_byte & 0x80)) {
            break;
        }
    }
    return value;
}

size_t AV1Track::write_leb128(uint64_t value, uint8_t *ptr, size_t len) {
    size_t i = 0;
    for (; 7 * i < 64;) {
        CHECK(i < len, "write_leb128 overflow");
        auto &byte = ptr[i++];
        byte = value & 0x7f;
        value >>= 7;
        if (!value) {
            break;
        }
        byte |= 0x80;
    }
    return i;
}

bool AV1Track::isConfigFrame(const char *data) {
    auto header = (AV1_ObuHeader *)data;
    return (AV1FrameType)header->obu_type == AV1FrameType::OBU_SEQUENCE_HEADER;
}

class AV1Sdp : public Sdp {
public:
    /**
     * 构造函数
     * @param payload_type  rtp payload type
     * @param bitrate 比特率
     */
    AV1Sdp(int payload_type, int bitrate, int profile, int level_idx, int tier = 1) : Sdp(90000, payload_type) {
        // 视频通道
        _printer << "m=video 0 RTP/AVP " << payload_type << "\r\n";
        if (bitrate) {
            _printer << "b=AS:" << bitrate << "\r\n";
        }
        _printer << "a=rtpmap:" << payload_type << " " << getCodecName(CodecAV1) << "/" << 90000 << "\r\n";
        // a=fmtp:98 profile=2; level-idx=8; tier=1;
        // https://aomediacodec.github.io/av1-rtp-spec/#73-examples
        _printer << "a=fmtp:" << payload_type << " profile=" << profile << "; level-idx=" << level_idx << "; tier=" << tier << "\r\n";
    }

    string getSdp() const override { return _printer; }

private:
    _StrPrinter _printer;
};

AV1Track::AV1Track() {
    _context = std::make_shared<AV1FrameOBUContext>();
    memset(_context.get(), 0, sizeof(AV1FrameOBUContext));
}

bool AV1Track::inputFrame_l(const Frame::Ptr &frame) {
    if (frame->configFrame()) {
        _config = std::make_shared<BufferOffset<Frame::Ptr>>(Frame::getCacheAbleFrame(frame), frame->prefixSize());
    }
    if (!ready()) {
        update();
    }
    return VideoTrack::inputFrame(frame);
}

toolkit::Buffer::Ptr AV1Track::getExtraData() const {
    CHECK(_config);

    std::string ret;
    ret.resize(AV1CodecConfigurationRecord::kOffset + _config->size());
    auto record = (AV1CodecConfigurationRecord *)ret.data();
    record->version = 1;
    record->marker = 1;
    record->seq_profile = _context->sequence_header_obu.seq_profile;
    record->seq_level_idx_0 = _context->sequence_header_obu.seq_level_idx[0];

    record->seq_tier_0 = _context->sequence_header_obu.seq_tier[0];
    record->high_bitdepth = _context->sequence_header_obu.color_config.high_bitdepth;
    record->twelve_bit = _context->sequence_header_obu.color_config.twelve_bit;
    record->monochrome = _context->sequence_header_obu.color_config.mono_chrome;
    record->chroma_subsampling_x = _context->sequence_header_obu.color_config.subsampling_x;
    record->chroma_subsampling_y = _context->sequence_header_obu.color_config.subsampling_y;
    record->chroma_sample_position = _context->sequence_header_obu.color_config.chroma_sample_position;

    record->reserved = 0;
    record->initial_presentation_delay_present = _context->sequence_header_obu.initial_display_delay_present_flag;
    record->initial_presentation_delay_minus_one = _context->sequence_header_obu.initial_display_delay_minus_1[0];
    memcpy(record->configOBUs, _config->data(), _config->size());
    return std::make_shared<BufferString>(std::move(ret));
}

void AV1Track::setExtraData(const uint8_t *data, size_t size) {
    CHECK(size > AV1CodecConfigurationRecord::kOffset);
    _config = std::make_shared<BufferString>(std::string((char *)data + AV1CodecConfigurationRecord::kOffset, size - AV1CodecConfigurationRecord::kOffset));
    update();
}

bool AV1Track::update() {
    if (!_config) {
        return false;
    }
    _context->load(_config->data(), _config->size());
    _width = _context->width;
    _height = _context->height;
    _fps = _context->fps;
    return true;
}

static Frame::Ptr addObuSize(const Frame::Ptr &frame) {
    auto size = frame->size();
    auto obu_header = (AV1_ObuHeader *)(frame->data());
    auto payload_size = size - 1 - obu_header->obu_extension_flag;

    uint8_t obu_size_buf[12];
    auto bytes = AV1Track::write_leb128(payload_size, obu_size_buf, sizeof(obu_size_buf));

    auto ret = FrameImp::create<AV1Frame>();
    ret->_prefix_size = 0;
    ret->_pts = frame->dts();
    ret->_dts = frame->pts();

    // obu_header and extension
    ret->_buffer.append((char *)obu_header, 1 + obu_header->obu_extension_flag);
    // obu_size leb128
    ret->_buffer.append((char *)obu_size_buf, bytes);
    // frame data
    ret->_buffer.append((char *)obu_header + 1 + obu_header->obu_extension_flag, payload_size);

    obu_header = (AV1_ObuHeader *)(ret->data());
    obu_header->obu_has_size_field = 1;
    return ret;
}

bool AV1Track::inputFrame(const Frame::Ptr &frame) {
    using AV1FrameInternal = FrameInternal<AV1FrameNoCacheAble>;
    bool ret = false;
    auto ptr = (uint8_t *)frame->data() + frame->prefixSize();
    auto end = (uint8_t *)frame->data() + frame->size();
    size_t frame_size;
    while (ptr + 1 < end) {
        auto obu_header = (AV1_ObuHeader *)(ptr++);
        if (obu_header->obu_extension_flag) {
            uint8_t temporal_idtemporal_id = (*ptr) >> 5;
            uint8_t spatial_idspatial_id = ((*ptr) >> 3) & 0x03;
            uint8_t extension_header_reserved_3 = (*ptr) & 0x07;
            ++ptr;
        }
        if (obu_header->obu_has_size_field) {
            size_t consumed;
            frame_size = leb128(ptr, end - ptr, consumed);
            ptr += (consumed + frame_size);
            frame_size += (1 + obu_header->obu_extension_flag + consumed);
            CHECK(ptr <= end, "Invalid frame size: ", frame_size);
        } else {
            frame_size = end - (uint8_t *)obu_header;
            ptr = end;
        }

        Frame::Ptr sub_frame = std::make_shared<AV1FrameInternal>(frame, (char *)obu_header, frame_size, 0);
        if (!obu_header->obu_has_size_field) {
            // 如果没有obu size字段，添加上它
            sub_frame = addObuSize(sub_frame);
        }

        if (inputFrame_l(sub_frame)) {
            ret = true;
        }
    }
    return ret;
}

Sdp::Ptr AV1Track::getSdp(uint8_t payload_type) const {
    if (!ready()) {
        WarnL << getCodecName() << " Track未准备好";
        return nullptr;
    }
    return std::make_shared<AV1Sdp>(payload_type, getBitRate() / 1024,
                                    _context->sequence_header_obu.seq_profile,
                                    _context->sequence_header_obu.seq_level_idx[0],
                                    _context->sequence_header_obu.seq_tier[0]);
}

namespace {

CodecId getCodec() {
    return CodecAV1;
}

Track::Ptr getTrackByCodecId(int sample_rate, int channels, int sample_bit) {
    return std::make_shared<AV1Track>();
}

Track::Ptr getTrackBySdp(const SdpTrack::Ptr &track) {
    return std::make_shared<AV1Track>();
}

RtpCodec::Ptr getRtpEncoderByCodecId(uint8_t pt) {
    return std::make_shared<AV1RtpEncoder>();
}

RtpCodec::Ptr getRtpDecoderByCodecId() {
    return std::make_shared<AV1RtpDecoder>();
}

RtmpCodec::Ptr getRtmpEncoderByTrack(const Track::Ptr &track) {
    return std::make_shared<VpxRtmpEncoder>(track);
}

RtmpCodec::Ptr getRtmpDecoderByTrack(const Track::Ptr &track) {
    return std::make_shared<VpxRtmpDecoder>(track);
}

Frame::Ptr getFrameFromPtr(const char *data, size_t bytes, uint64_t dts, uint64_t pts) {
    return std::make_shared<AV1FrameNoCacheAble>((char *)data, bytes, dts, pts);
}

} // namespace

CodecPlugin av1_plugin = { getCodec,
                           getTrackByCodecId,
                           getTrackBySdp,
                           getRtpEncoderByCodecId,
                           getRtpDecoderByCodecId,
                           getRtmpEncoderByTrack,
                           getRtmpDecoderByTrack,
                           getFrameFromPtr };

} // namespace mediakit