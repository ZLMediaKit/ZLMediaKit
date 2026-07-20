/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "H265.h"
#include "H265Rtp.h"
#include "H265Rtmp.h"
#include "Util/base64.h"
#include "Common/Parser.h"
#include "Extension/Factory.h"

#include <vector>
#include <stdexcept>
#include <climits>

#ifdef ENABLE_MP4
#include "mpeg4-hevc.h"
#endif

using namespace std;
using namespace toolkit;

namespace mediakit {

// ---- 内部比特流工具（H265） ----
namespace {

// VPS/SPS 的实际语法规模远小于 1 MiB；在复制 RBSP 前设置宽松上限，防止恶意参数集制造同等规模的额外分配。
// Practical VPS/SPS syntax is far smaller than 1 MiB; a generous pre-copy cap prevents hostile parameter sets from forcing an equal-sized allocation.
static constexpr size_t kMaxParameterSetSize = 1024 * 1024;

static std::vector<uint8_t> h265_rbsp_from_nalu(const uint8_t *data, size_t size) {
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

struct H265BS {
    const uint8_t *buf;
    size_t size;
    size_t pos;
    H265BS(const uint8_t *b, size_t s) : buf(b), size(s), pos(0) {}
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
    uint32_t read_ue() {
        int z = 0;
        // Exp-Golomb 编码必须包含值为 1 的停止位；此前在停止位前遇到 EOF 会被误判为 0，并掩盖 SPS 截断。
        // Exp-Golomb codes require a one-bit terminator; treating EOF before it as zero concealed truncated SPS data.
        while (true) {
            if (eof()) {
                throw std::runtime_error("eof before exp-golomb stop bit");
            }
            if (read_bits(1) != 0) {
                break;
            }
            // 本读取器返回 uint32_t，最多只能接受 31 个前导零；32 个前导零属于无法表示的 33 位 ue(v) 编码。
            // This uint32_t reader accepts at most 31 leading zeroes; 32 form a 33-bit ue(v) code that cannot be represented here.
            if (++z >= 32) {
                throw std::runtime_error("exp-golomb overflow");
            }
        }
        if (z == 0) return 0;
        return (1u << z) - 1 + read_bits(z);
    }
    int32_t read_se() {
        uint32_t v = read_ue();
        return (v & 1) ? (int32_t)((v + 1) >> 1) : -(int32_t)(v >> 1);
    }

    size_t rbsp_stop_bit_position() const {
        size_t rbsp_end = size * 8;
        // 项目既有输入偶尔在参数集末尾保留 Annex-B 起始码；判断扩展数据时先排除该终止分隔符。
        // Existing project inputs occasionally retain a terminal Annex-B start code; exclude that delimiter when locating extension data.
        if (size >= 3 && buf[size - 3] == 0 && buf[size - 2] == 0 && buf[size - 1] == 1) {
            rbsp_end -= 24;
        }
        if (pos >= rbsp_end) {
            throw std::runtime_error("missing rbsp stop bit");
        }
        // 从尾部一次定位停止位，避免逐位调用 more_rbsp_data() 导致长扩展数据退化为 O(n²)。
        // Locate the stop bit once from the end so long extension data cannot turn repeated more_rbsp_data() scans into O(n²).
        for (size_t i = rbsp_end; i-- > pos;) {
            if ((buf[i / 8] >> (7 - i % 8)) & 1) {
                return i;
            }
        }
        throw std::runtime_error("missing rbsp stop bit");
    }

    void skip_extension_data() {
        pos = rbsp_stop_bit_position();
    }

    void read_rbsp_trailing_bits() {
        // 必须读到 rbsp_stop_one_bit 和对齐零位，防止仅解析出宽高便把截断的 VPS/SPS 当作完整参数集。
        // Require the stop bit and alignment zeroes so a VPS/SPS truncated after dimensions cannot be mistaken for a complete parameter set.
        if (read_bits(1) != 1) {
            throw std::runtime_error("invalid rbsp stop bit");
        }
        while (pos & 7) {
            if (read_bits(1) != 0) {
                throw std::runtime_error("invalid rbsp alignment bit");
            }
        }
        // 兼容 trailing_zero_8bits 与末尾 Annex-B 起始码，但拒绝其他未解析的非零尾部数据。
        // Accept trailing_zero_8bits and a terminal Annex-B start code for compatibility, while rejecting other unparsed non-zero tails.
        size_t zero_bytes = 0;
        while (!eof()) {
            uint32_t byte = read_bits(8);
            if (byte == 0) {
                ++zero_bytes;
                continue;
            }
            if (byte == 1 && zero_bytes >= 2 && eof()) {
                return;
            }
            throw std::runtime_error("invalid data after rbsp trailing bits");
        }
    }

    // profile_tier_level(profilePresentFlag, maxNumSubLayersMinus1)
    void skip_profile_tier_level(bool profilePresentFlag, uint32_t maxNumSubLayersMinus1) {
        if (profilePresentFlag) {
            skip_bits(2 + 1 + 5); // profile_space + tier_flag + profile_idc
            skip_bits(32);        // profile_compatibility_flag[32]
            skip_bits(4);         // progressive/interlaced/non_packed/frame_only
            skip_bits(44);        // reserved_zero_44bits
        }
        skip_bits(8); // general_level_idc
        // sub_layer flags
        std::vector<bool> profile_present(maxNumSubLayersMinus1), level_present(maxNumSubLayersMinus1);
        for (uint32_t i = 0; i < maxNumSubLayersMinus1; i++) {
            profile_present[i] = read_bits(1) != 0;
            level_present[i]   = read_bits(1) != 0;
        }
        if (maxNumSubLayersMinus1 > 0) {
            for (uint32_t i = maxNumSubLayersMinus1; i < 8; i++) skip_bits(2);
        }
        for (uint32_t i = 0; i < maxNumSubLayersMinus1; i++) {
            if (profile_present[i]) {
                skip_bits(2 + 1 + 5 + 32 + 4 + 44);
            }
            if (level_present[i]) skip_bits(8);
        }
    }
};

struct H265HrdState {
    bool nal_parameters_present = false;
    bool vcl_parameters_present = false;
    bool sub_pic_parameters_present = false;
};

static void skip_h265_sub_layer_hrd_parameters(H265BS &bs, uint32_t cpb_cnt_minus1,
                                                bool sub_pic_parameters_present) {
    for (uint32_t i = 0; i <= cpb_cnt_minus1; ++i) {
        bs.read_ue(); // bit_rate_value_minus1
        bs.read_ue(); // cpb_size_value_minus1
        if (sub_pic_parameters_present) {
            bs.read_ue(); // cpb_size_du_value_minus1
            bs.read_ue(); // bit_rate_du_value_minus1
        }
        bs.skip_bits(1); // cbr_flag
    }
}

static void skip_h265_hrd_parameters(H265BS &bs, bool common_info_present,
                                     uint32_t max_sub_layers_minus1, H265HrdState &state) {
    if (common_info_present) {
        state.nal_parameters_present = bs.read_bits(1) != 0;
        state.vcl_parameters_present = bs.read_bits(1) != 0;
        state.sub_pic_parameters_present = false;
        if (state.nal_parameters_present || state.vcl_parameters_present) {
            state.sub_pic_parameters_present = bs.read_bits(1) != 0;
            if (state.sub_pic_parameters_present) {
                bs.skip_bits(8 + 5 + 1 + 5); // sub-picture HRD lengths and flags
            }
            bs.skip_bits(8); // bit_rate_scale + cpb_size_scale
            if (state.sub_pic_parameters_present) {
                bs.skip_bits(4); // cpb_size_du_scale
            }
            bs.skip_bits(15); // initial/au/dpb delay lengths
        }
    }

    for (uint32_t i = 0; i <= max_sub_layers_minus1; ++i) {
        bool fixed_pic_rate_general = bs.read_bits(1) != 0;
        bool fixed_pic_rate_within_cvs = fixed_pic_rate_general || bs.read_bits(1) != 0;
        bool low_delay_hrd = false;
        if (fixed_pic_rate_within_cvs) {
            bs.read_ue(); // elemental_duration_in_tc_minus1
        } else {
            low_delay_hrd = bs.read_bits(1) != 0;
        }
        uint32_t cpb_cnt_minus1 = low_delay_hrd ? 0 : bs.read_ue();
        // HEVC 每个子层最多定义 32 个 CPB；先校验计数，避免 HRD 循环被恶意 ue(v) 无界放大。
        // HEVC defines at most 32 CPBs per sub-layer; validate the count before an untrusted ue(v) can amplify HRD loops.
        if (cpb_cnt_minus1 > 31) {
            throw std::runtime_error("invalid h265 cpb count");
        }
        if (state.nal_parameters_present) {
            skip_h265_sub_layer_hrd_parameters(bs, cpb_cnt_minus1, state.sub_pic_parameters_present);
        }
        if (state.vcl_parameters_present) {
            skip_h265_sub_layer_hrd_parameters(bs, cpb_cnt_minus1, state.sub_pic_parameters_present);
        }
    }
}

static void skip_h265_sps_3d_extension(H265BS &bs, uint32_t min_cb_log2_size_y,
                                       uint32_t ctb_log2_size_y) {
    for (uint32_t d = 0; d <= 1; ++d) {
        bs.skip_bits(2); // iv_di_mc_enabled_flag + iv_mv_scal_enabled_flag
        uint32_t sub_pb_size_minus3;
        if (d == 0) {
            sub_pb_size_minus3 = bs.read_ue(); // log2_ivmc_sub_pb_size_minus3
            bs.skip_bits(4); // iv_res_pred/depth_ref/vsp_mc/dbbp flags
        } else {
            bs.skip_bits(1); // tex_mc_enabled_flag
            sub_pb_size_minus3 = bs.read_ue(); // log2_texmc_sub_pb_size_minus3
            bs.skip_bits(5); // intra_contour/intra_dc_only_wedge/cqt/inter_dc_only/skip_intra flags
        }
        // 3D 子块尺寸必须位于当前 SPS 派生的编码块范围内；先解析并校验，才能区分字段中的 1 与真正的 RBSP 停止位。
        // 3D sub-block sizes must stay within the coding-block range derived by this SPS; parsing and validating them distinguishes field bits from the real RBSP stop bit.
        if (sub_pb_size_minus3 < min_cb_log2_size_y - 3 || sub_pb_size_minus3 > ctb_log2_size_y - 3) {
            throw std::runtime_error("invalid h265 3d sub-block size");
        }
    }
}

static void skip_h265_sps_scc_extension(H265BS &bs, uint32_t chroma_format_idc,
                                        uint32_t bit_depth_luma, uint32_t bit_depth_chroma) {
    bs.skip_bits(1); // sps_curr_pic_ref_enabled_flag
    if (bs.read_bits(1)) { // palette_mode_enabled_flag
        uint32_t palette_max_size = bs.read_ue();
        uint32_t delta_palette_max_predictor_size = bs.read_ue();
        // SCC 配置最多定义 64 个调色板项和 128 个预测项；限制派生总量，避免不可信计数放大初始化器循环。
        // SCC profiles define at most 64 palette entries and 128 predictor entries; cap the derived total before untrusted counts amplify initializer loops.
        uint64_t palette_max_predictor_size =
            (uint64_t)palette_max_size + delta_palette_max_predictor_size;
        if (palette_max_size > 64 || delta_palette_max_predictor_size > 128 ||
            palette_max_predictor_size > 128 || (palette_max_size == 0 && delta_palette_max_predictor_size != 0)) {
            throw std::runtime_error("invalid h265 palette size");
        }
        if (bs.read_bits(1)) { // sps_palette_predictor_initializers_present_flag
            uint32_t initializer_count_minus1 = bs.read_ue();
            uint64_t initializer_count = (uint64_t)initializer_count_minus1 + 1;
            if (initializer_count > 128 || initializer_count > palette_max_predictor_size) {
                throw std::runtime_error("invalid h265 palette initializer count");
            }
            uint32_t component_count = chroma_format_idc == 0 ? 1 : 3;
            for (uint32_t component = 0; component < component_count; ++component) {
                uint32_t bit_depth = component == 0 ? bit_depth_luma : bit_depth_chroma;
                for (uint32_t i = 0; i < initializer_count; ++i) {
                    bs.skip_bits((int)bit_depth); // sps_palette_predictor_initializer
                }
            }
        }
    }
    uint32_t motion_vector_resolution_control_idc = bs.read_bits(2);
    if (motion_vector_resolution_control_idc == 3) {
        throw std::runtime_error("reserved h265 motion-vector resolution control");
    }
    bs.skip_bits(1); // intra_boundary_filtering_disabled_flag
}

} // anonymous namespace

// ---- H265 VPS 解析（只提取帧率用的 timing info） ----
static bool parse_hevc_vps_fps(const uint8_t *data, size_t size, float &fps) {
    // data 为 NALU 原始数据（含 NAL header）
    if (size < 3 || size > kMaxParameterSetSize) return false;
    try {
        // RBSP 分配必须处于异常保护内，避免超大恶意 VPS 让 bad_alloc 逃出布尔解析接口。
        // Keep RBSP allocation inside the guard so a huge hostile VPS cannot leak bad_alloc through the boolean parser API.
        auto rbsp = h265_rbsp_from_nalu(data, size);
        H265BS bs(rbsp.data(), rbsp.size());
        float parsed_fps = fps;
        // NALU header: forbidden_zero_bit(1) + nal_unit_type(6) + nuh_layer_id(6) + nuh_temporal_id_plus1(3)
        bs.skip_bits(16);
        // vps_video_parameter_set_id(4) + vps_reserved_three_2bits(2) + vps_max_layers_minus1(6)
        bs.skip_bits(4 + 2 + 6);
        uint32_t vps_max_sub_layers_minus1 = bs.read_bits(3);
        // HEVC 最多定义 7 个时间子层，因此 minus1 字段只允许 0..6；值 7 为保留值，不能继续控制后续循环。
        // HEVC defines at most seven temporal sub-layers, so the minus-one field is limited to 0..6; reserved value 7 must not control later loops.
        if (vps_max_sub_layers_minus1 > 6) {
            return false;
        }
        bs.skip_bits(1); // vps_temporal_id_nesting_flag
        bs.skip_bits(16); // vps_reserved_0xffff_16bits
        bs.skip_profile_tier_level(true, vps_max_sub_layers_minus1);
        bool vps_sub_layer_ordering_info_present_flag = bs.read_bits(1) != 0;
        uint32_t start = vps_sub_layer_ordering_info_present_flag ? 0 : vps_max_sub_layers_minus1;
        for (uint32_t i = start; i <= vps_max_sub_layers_minus1; i++) {
            bs.read_ue(); // vps_max_dec_pic_buffering_minus1
            bs.read_ue(); // vps_max_num_reorder_pics
            bs.read_ue(); // vps_max_latency_increase_plus1
        }
        uint32_t vps_max_layer_id = bs.read_bits(6);
        // nuh_layer_id 的有效最大值为 62；63 是保留值，不能用于放大 layer-set 标志循环。
        // The largest valid nuh_layer_id is 62; reserved value 63 must not amplify the layer-set flag loop.
        if (vps_max_layer_id > 62) {
            return false;
        }
        uint32_t vps_num_layer_sets_minus1 = bs.read_ue();
        // 标准上限为 1023，并且每个 layer flag 都必须实际存在；预检可避免恶意计数长时间占用输入线程。
        // The standard limit is 1023 and every layer flag must be present; preflight checks prevent hostile counts from stalling the input thread.
        uint64_t layer_flag_count = (uint64_t)vps_num_layer_sets_minus1 * (vps_max_layer_id + 1);
        if (vps_num_layer_sets_minus1 > 1023 || layer_flag_count > bs.bits_left()) {
            return false;
        }
        for (uint32_t i = 1; i <= vps_num_layer_sets_minus1; i++) {
            for (uint32_t j = 0; j <= vps_max_layer_id; j++) bs.skip_bits(1);
        }
        if (bs.read_bits(1)) { // vps_timing_info_present_flag
            uint32_t vps_num_units_in_tick = bs.read_bits(32);
            uint32_t vps_time_scale        = bs.read_bits(32);
            if (vps_num_units_in_tick > 0) {
                parsed_fps = (float)vps_time_scale / (float)vps_num_units_in_tick;
            }
            if (bs.read_bits(1)) { // vps_poc_proportional_to_timing_flag
                bs.read_ue(); // vps_num_ticks_poc_diff_one_minus1
            }
            uint32_t vps_num_hrd_parameters = bs.read_ue();
            // 每个 HRD 条目必须引用已声明的 layer set；上限校验同时约束后续嵌套子层/CPB 循环。
            // Every HRD entry must reference a declared layer set; this limit also bounds the following nested sub-layer/CPB loops.
            if ((uint64_t)vps_num_hrd_parameters > (uint64_t)vps_num_layer_sets_minus1 + 1) {
                return false;
            }
            H265HrdState hrd_state;
            for (uint32_t i = 0; i < vps_num_hrd_parameters; ++i) {
                uint32_t hrd_layer_set_idx = bs.read_ue();
                if (hrd_layer_set_idx > vps_num_layer_sets_minus1) {
                    return false;
                }
                bool common_info_present = i == 0 || bs.read_bits(1) != 0;
                skip_h265_hrd_parameters(bs, common_info_present, vps_max_sub_layers_minus1, hrd_state);
            }
        }
        if (bs.read_bits(1)) { // vps_extension_flag
            // VPS 扩展不参与帧率提取；按规范消费 extension_data_flag 至停止位，既保持扩展兼容性，也能验证参数集完整结束。
            // VPS extensions do not affect extracted timing; consume extension_data_flag up to the stop bit to stay compatible and still validate completion.
            bs.skip_extension_data();
        }
        bs.read_rbsp_trailing_bits();
        // 帧率仅在所有尾部语法与停止位验证后写回，避免截断 VPS 发布部分解析结果。
        // Publish timing only after all tail syntax and the stop bit validate, so a truncated VPS cannot leak partial output.
        fps = parsed_fps;
        return true;
    } catch (...) {
        return false;
    }
}

// ---- H265 SPS 解析（宽高 + 备用帧率） ----
static bool parse_hevc_sps(const uint8_t *data, size_t size,
                            int &width, int &height, float &fps) {
    if (size < 3 || size > kMaxParameterSetSize) return false;
    try {
        // RBSP 分配也属于解析失败路径；纳入异常保护才能保证 malformed input 统一返回 false。
        // RBSP allocation is part of parsing failure; guard it so malformed input consistently returns false.
        auto rbsp = h265_rbsp_from_nalu(data, size);
        H265BS bs(rbsp.data(), rbsp.size());
        int parsed_width = 0;
        int parsed_height = 0;
        float parsed_fps = fps;
        bs.skip_bits(16); // NALU header
        bs.skip_bits(4);  // sps_video_parameter_set_id
        uint32_t sps_max_sub_layers_minus1 = bs.read_bits(3);
        // 与 VPS 相同，SPS 的时间子层 minus1 字段只允许 0..6；先拒绝保留值 7，避免错误位移和循环次数。
        // As in the VPS, the SPS temporal-sub-layer minus-one field is limited to 0..6; reject reserved 7 before it skews offsets and loop counts.
        if (sps_max_sub_layers_minus1 > 6) {
            return false;
        }
        bs.skip_bits(1);  // sps_temporal_id_nesting_flag
        bs.skip_profile_tier_level(true, sps_max_sub_layers_minus1);
        uint32_t sps_seq_parameter_set_id = bs.read_ue();
        // HEVC SPS id 的标准范围为 0..15；拒绝 16，避免接受参数集表无法索引的配置。
        // HEVC SPS ids are defined in 0..15; reject 16 instead of accepting configuration that the parameter-set table cannot index.
        if (sps_seq_parameter_set_id > 15) {
            return false;
        }
        uint32_t chroma_format_idc = bs.read_ue();
        if (chroma_format_idc > 3) {
            return false;
        }
        if (chroma_format_idc == 3) bs.skip_bits(1); // separate_colour_plane_flag
        uint32_t pic_width  = bs.read_ue();
        uint32_t pic_height = bs.read_ue();

        if (bs.read_bits(1)) { // conformance_window_flag
            uint32_t sub_width_c  = (chroma_format_idc == 1 || chroma_format_idc == 2) ? 2 : 1;
            uint32_t sub_height_c = (chroma_format_idc == 1) ? 2 : 1;
            // crop offset 由不可信 UE 值控制；使用 uint64_t 完成乘加，避免 uint32_t 回绕后绕过边界检查。
            // Crop offsets come from untrusted UE values; wide multiplication and addition prevent uint32_t wraparound from bypassing bounds checks.
            uint64_t crop_left   = (uint64_t)bs.read_ue() * sub_width_c;
            uint64_t crop_right  = (uint64_t)bs.read_ue() * sub_width_c;
            uint64_t crop_top    = (uint64_t)bs.read_ue() * sub_height_c;
            uint64_t crop_bottom = (uint64_t)bs.read_ue() * sub_height_c;
            uint64_t crop_width = crop_left + crop_right;
            uint64_t crop_height = crop_top + crop_bottom;
            if (crop_width >= pic_width || crop_height >= pic_height) {
                return false;
            }
            pic_width -= (uint32_t)crop_width;
            pic_height -= (uint32_t)crop_height;
        }

        // 输出接口使用 int；转换前检查范围，避免超大尺寸产生实现定义的窄化结果。
        // The output API uses int; range-check before narrowing to avoid implementation-defined results for oversized dimensions.
        if (pic_width == 0 || pic_height == 0 || pic_width > INT_MAX || pic_height > INT_MAX) {
            return false;
        }
        parsed_width = (int)pic_width;
        parsed_height = (int)pic_height;

        uint32_t bit_depth_luma_minus8 = bs.read_ue();
        uint32_t bit_depth_chroma_minus8 = bs.read_ue();
        // HEVC 分别定义亮度和色度位深增量，两者各自限制为 0..8 但不要求相等；这里只校验参与派生计算所需的独立边界。
        // HEVC defines separate luma and chroma bit-depth offsets, each limited to 0..8 without an equality requirement; validate only their independent bounds before derived arithmetic.
        if (bit_depth_luma_minus8 > 8 || bit_depth_chroma_minus8 > 8) {
            return false;
        }
        uint32_t log2_max_pic_order_cnt_lsb_minus4 = bs.read_ue();
        // 该值标准范围为 0..12；限制后续 skip_bits 参数可表示且不会被恶意值扭曲。
        // Its standard range is 0..12; enforcing it keeps later skip_bits counts representable and input-safe.
        if (log2_max_pic_order_cnt_lsb_minus4 > 12) {
            return false;
        }

        bool sps_sub_layer_ordering_info_present_flag = bs.read_bits(1) != 0;
        uint32_t start = sps_sub_layer_ordering_info_present_flag ? 0 : sps_max_sub_layers_minus1;
        for (uint32_t i = start; i <= sps_max_sub_layers_minus1; i++) {
            bs.read_ue(); bs.read_ue(); bs.read_ue();
        }
        uint32_t log2_min_luma_coding_block_size_minus3 = bs.read_ue();
        uint32_t log2_diff_max_min_luma_coding_block_size = bs.read_ue();
        // 两个字段各自仅允许 0..3；校验后再派生编码块范围，避免恶意 ue(v) 参与加法并污染 3D 扩展边界。
        // Both fields are limited to 0..3; validate before deriving the coding-block range so hostile ue(v) values cannot enter addition or 3D-extension bounds.
        if (log2_min_luma_coding_block_size_minus3 > 3 ||
            log2_diff_max_min_luma_coding_block_size > 3) {
            return false;
        }
        uint32_t min_cb_log2_size_y = log2_min_luma_coding_block_size_minus3 + 3;
        uint32_t ctb_log2_size_y = min_cb_log2_size_y + log2_diff_max_min_luma_coding_block_size;
        bs.read_ue(); // log2_min_luma_transform_block_size_minus2
        bs.read_ue(); // log2_diff_max_min_luma_transform_block_size
        bs.read_ue(); // max_transform_hierarchy_depth_inter
        bs.read_ue(); // max_transform_hierarchy_depth_intra

        if (bs.read_bits(1)) { // scaling_list_enabled_flag
            if (bs.read_bits(1)) { // sps_scaling_list_data_present_flag
                for (int sizeId = 0; sizeId < 4; sizeId++) {
                    for (int matrixId = 0; matrixId < (sizeId == 3 ? 2 : 6); matrixId++) {
                        if (!bs.read_bits(1)) { // scaling_list_pred_mode_flag
                            bs.read_ue(); // scaling_list_pred_matrix_id_delta
                        } else {
                            int coefNum = (std::min)(64, 1 << (4 + (sizeId << 1)));
                            if (sizeId > 1) bs.read_se(); // scaling_list_dc_coef_minus8
                            for (int i = 0; i < coefNum; i++) bs.read_se();
                        }
                    }
                }
            }
        }

        bs.skip_bits(2); // amp_enabled_flag + sample_adaptive_offset_enabled_flag
        if (bs.read_bits(1)) { // pcm_enabled_flag
            bs.skip_bits(4 + 4); // pcm_sample_bit_depth_luma/chroma_minus1
            bs.read_ue(); bs.read_ue(); // log2_min/max pcm_luma_coding_block_size
            bs.skip_bits(1); // pcm_loop_filter_disabled_flag
        }

        uint32_t num_short_term_ref_pic_sets = bs.read_ue();
        // 标准最多允许 64 个短期 RPS；循环前拒绝超限值，避免参数集放大媒体线程工作量。
        // The standard permits at most 64 short-term RPS entries; reject larger counts before they amplify media-thread work.
        if (num_short_term_ref_pic_sets > 64) {
            return false;
        }
        uint32_t prev_num_delta_pocs = 0;
        for (uint32_t i = 0; i < num_short_term_ref_pic_sets; i++) {
            bool inter_ref = (i != 0) && bs.read_bits(1) != 0;
            if (inter_ref) {
                bs.skip_bits(1); // delta_rps_sign
                bs.read_ue();    // abs_delta_rps_minus1
                uint32_t n = prev_num_delta_pocs + 1;
                uint32_t cnt = 0;
                for (uint32_t j = 0; j < n; j++) {
                    bool used = bs.read_bits(1) != 0;
                    bool use  = !used && bs.read_bits(1) != 0;
                    if (used || use) cnt++;
                }
                // 单个 RPS 最多容纳 32 个 delta POC；限制派生计数，防止后续循环被恶意状态持续放大。
                // A single RPS holds at most 32 delta POCs; cap the derived count before it controls the next input-driven loop.
                if (cnt > 32) {
                    return false;
                }
                prev_num_delta_pocs = cnt;
            } else {
                uint32_t num_neg = bs.read_ue();
                uint32_t num_pos = bs.read_ue();
                // 旧实现和标准数据结构均限制每类参考图像少于 16；先校验可同时避免加法回绕。
                // The prior parser and standard data model limit each reference class to fewer than 16; checking first also prevents addition wraparound.
                if (num_neg >= 16 || num_pos >= 16) {
                    return false;
                }
                prev_num_delta_pocs = num_neg + num_pos;
                for (uint32_t j = 0; j < num_neg; j++) { bs.read_ue(); bs.skip_bits(1); }
                for (uint32_t j = 0; j < num_pos; j++) { bs.read_ue(); bs.skip_bits(1); }
            }
        }

        if (bs.read_bits(1)) { // long_term_ref_pics_present_flag
            uint32_t n = bs.read_ue();
            // 标准最多允许 32 个长期参考图像；在循环前校验，避免恶意计数阻塞输入线程。
            // The standard permits at most 32 long-term references; validate before looping to keep hostile counts off the input thread.
            if (n > 32) {
                return false;
            }
            uint32_t log2_max = log2_max_pic_order_cnt_lsb_minus4 + 4;
            for (uint32_t i = 0; i < n; i++) {
                bs.skip_bits(log2_max); // lt_ref_pic_poc_lsb_sps
                bs.skip_bits(1);        // used_by_curr_pic_lt_sps_flag
            }
        }

        bs.skip_bits(2); // sps_temporal_mvp_enabled_flag + strong_intra_smoothing_enabled_flag

        if (bs.read_bits(1)) { // vui_parameters_present_flag
            if (bs.read_bits(1)) { // aspect_ratio_info_present_flag
                if (bs.read_bits(8) == 255) bs.skip_bits(32);
            }
            if (bs.read_bits(1)) bs.skip_bits(1); // overscan
            if (bs.read_bits(1)) { // video_signal_type_present_flag
                bs.skip_bits(3 + 1);
                if (bs.read_bits(1)) bs.skip_bits(24);
            }
            if (bs.read_bits(1)) { bs.read_ue(); bs.read_ue(); } // chroma_loc_info
            bs.skip_bits(3); // neutral_chroma/field_seq/frame_field_info
            if (bs.read_bits(1)) { // default_display_window_flag
                bs.read_ue(); // def_disp_win_left_offset
                bs.read_ue(); // def_disp_win_right_offset
                bs.read_ue(); // def_disp_win_top_offset
                bs.read_ue(); // def_disp_win_bottom_offset
            }
            if (bs.read_bits(1)) { // vui_timing_info_present_flag
                uint32_t num_units = bs.read_bits(32);
                uint32_t time_scale = bs.read_bits(32);
                if (num_units > 0 && parsed_fps <= 0.0f) {
                    parsed_fps = (float)time_scale / (float)num_units;
                }
                if (bs.read_bits(1)) { // vui_poc_proportional_to_timing_flag
                    bs.read_ue(); // vui_num_ticks_poc_diff_one_minus1
                }
                if (bs.read_bits(1)) { // vui_hrd_parameters_present_flag
                    H265HrdState hrd_state;
                    skip_h265_hrd_parameters(bs, true, sps_max_sub_layers_minus1, hrd_state);
                }
            }
            if (bs.read_bits(1)) { // bitstream_restriction_flag
                bs.skip_bits(3); // tiles_fixed_structure/motion_vectors/restricted_ref_pic_lists flags
                bs.read_ue(); // min_spatial_segmentation_idc
                bs.read_ue(); // max_bytes_per_pic_denom
                bs.read_ue(); // max_bits_per_min_cu_denom
                bs.read_ue(); // log2_max_mv_length_horizontal
                bs.read_ue(); // log2_max_mv_length_vertical
            }
        }
        if (bs.read_bits(1)) { // sps_extension_present_flag
            bool range_extension = bs.read_bits(1) != 0;
            bool multilayer_extension = bs.read_bits(1) != 0;
            bool extension_3d = bs.read_bits(1) != 0;
            bool scc_extension = bs.read_bits(1) != 0;
            uint32_t extension_4bits = bs.read_bits(4);
            if (range_extension) {
                bs.skip_bits(9); // sps_range_extension flags
            }
            if (multilayer_extension) {
                bs.skip_bits(1); // inter_view_mv_vert_constraint_flag
            }
            if (extension_3d) {
                skip_h265_sps_3d_extension(bs, min_cb_log2_size_y, ctb_log2_size_y);
            }
            if (scc_extension) {
                skip_h265_sps_scc_extension(bs, chroma_format_idc,
                                            bit_depth_luma_minus8 + 8, bit_depth_chroma_minus8 + 8);
            }
            if (extension_4bits) {
                // 只有 sps_extension_4bits 对应的未来语法才是无结构 extension_data_flag；已标准化扩展必须逐字段消费，否则截断字段中的最后一个 1 会被误认成停止位。
                // Only future syntax selected by sps_extension_4bits is unstructured extension_data_flag; standardized extensions must be consumed field by field or a final one-bit field in truncated data can masquerade as the stop bit.
                bs.skip_extension_data();
            }
        }
        bs.read_rbsp_trailing_bits();
        // 完成必需字段解析后再提交结果，避免截断 SPS 发布仅解析了一半的宽高。
        // Commit only after all required fields parse so truncated SPS data cannot publish half-validated dimensions.
        width = parsed_width;
        height = parsed_height;
        fps = parsed_fps;
        return true;
    } catch (...) {
        return false;
    }
}

bool getHEVCInfo(const char *vps, size_t vps_len, const char *sps, size_t sps_len,
                 int &iVideoWidth, int &iVideoHeight, float &iVideoFps) {
    int parsed_width = 0;
    int parsed_height = 0;
    float parsed_fps = 0.0f;

    // 先从 VPS 提取帧率
    if (vps_len > 2) {
        parse_hevc_vps_fps((const uint8_t *)vps, vps_len, parsed_fps);
    }

    // 再从 SPS 提取宽高（如果 VPS 没有帧率，SPS VUI 里也可能有）
    if (sps_len <= 2) return false;
    if (!parse_hevc_sps((const uint8_t *)sps, sps_len, parsed_width, parsed_height, parsed_fps)) {
        return false;
    }
    // 对外参数只在 VPS/SPS 解析成功后再依次发布，确保失败不会清空或污染调用方已有元数据；这不是跨线程原子更新。
    // Publish public outputs only after VPS/SPS parsing succeeds so failure preserves caller metadata; these assignments are not cross-thread atomic.
    iVideoWidth = parsed_width;
    iVideoHeight = parsed_height;
    iVideoFps = parsed_fps;
    return true;
}

bool getHEVCInfo(const string &strVps, const string &strSps, int &iVideoWidth, int &iVideoHeight, float &iVideoFps) {
    return getHEVCInfo(strVps.data(), strVps.size(), strSps.data(), strSps.size(), iVideoWidth, iVideoHeight,iVideoFps);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

H265Track::H265Track(const string &vps,const string &sps, const string &pps,int vps_prefix_len, int sps_prefix_len, int pps_prefix_len) {
    _vps = vps.substr(vps_prefix_len);
    _sps = sps.substr(sps_prefix_len);
    _pps = pps.substr(pps_prefix_len);
    H265Track::update();
}

CodecId H265Track::getCodecId() const {
    return CodecH265;
}

int H265Track::getVideoHeight() const {
    return _height;
}

int H265Track::getVideoWidth() const {
    return _width;
}

float H265Track::getVideoFps() const {
    return _fps;
}

bool H265Track::ready() const {
    return !_vps.empty() && !_sps.empty() && !_pps.empty();
}

bool H265Track::inputFrame(const Frame::Ptr &frame) {
    int type = H265_TYPE(frame->data()[frame->prefixSize()]);
    if (!frame->configFrame() && type != H265Frame::NAL_SEI_PREFIX && ready()) {
        return inputFrame_l(frame);
    }
    bool ret = false;
    splitH264(frame->data(), frame->size(), frame->prefixSize(), [&](const char *ptr, size_t len, size_t prefix) {
        using H265FrameInternal = FrameInternal<H265FrameNoCacheAble>;
        H265FrameInternal::Ptr sub_frame = std::make_shared<H265FrameInternal>(frame, (char *) ptr, len, prefix);
        if (inputFrame_l(sub_frame)) {
            ret = true;
        }
    });
    return ret;
}

bool H265Track::inputFrame_l(const Frame::Ptr &frame) {
    int type = H265_TYPE(frame->data()[frame->prefixSize()]);
    bool was_ready = ready();
    bool ret = true;
    switch (type) {
        case H265Frame::NAL_VPS: {
            _vps = string(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
            _latest_is_config_frame = true;
            ret = VideoTrack::inputFrame(frame);
            break;
        }
        case H265Frame::NAL_SPS: {
            _sps = string(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
            _latest_is_config_frame = true;
            ret = VideoTrack::inputFrame(frame);
            break;
        }
        case H265Frame::NAL_PPS: {
            _pps = string(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
            _latest_is_config_frame = true;
            ret = VideoTrack::inputFrame(frame);
            break;
        }
        default: {
            // 判断是否是I帧, 并且如果是,那判断前面是否插入过config帧, 如果插入过就不插入了
            if (frame->keyFrame() && !_latest_is_config_frame) {
                insertConfigFrame(frame);
            }
            if (!frame->dropAble()) {
                _latest_is_config_frame = false;
            }
            ret = VideoTrack::inputFrame(frame);
            break;
        }
    }
    // 仅当 SPS 改变或本帧首次补齐配置时重试：宽高解析失败后，重复 VPS/PPS 无法改变 SPS 结果，只会在媒体线程重复做无效工作。
    // Retry only when the SPS changes or this frame first completes configuration: after dimension parsing fails, repeated VPS/PPS cannot change the SPS result and only repeat work on the media thread.
    bool configuration_became_ready = !was_ready && ready();
    if (_width == 0 && ready() && (type == H265Frame::NAL_SPS || configuration_became_ready)) {
        update();
    }
    return ret;
}

toolkit::Buffer::Ptr H265Track::getExtraData() const {
    CHECK(ready());
#ifdef ENABLE_MP4
    struct mpeg4_hevc_t hevc;
    memset(&hevc, 0, sizeof(hevc));
    // mpeg4_hevc_t 使用固定数组保存 VPS/SPS/PPS，第三方转换器在总长度超限时会触发断言；逐项减法检查既避免加法溢出，也把失败限制在本 Track 内。
    // mpeg4_hevc_t stores VPS/SPS/PPS in a fixed array and its converter asserts when their total size exceeds it; staged subtraction avoids overflow and keeps failure in this Track.
    if (_vps.size() > sizeof(hevc.data) || _sps.size() > sizeof(hevc.data) - _vps.size() ||
        _pps.size() > sizeof(hevc.data) - _vps.size() - _sps.size()) {
        WarnL << "H265参数集过大，无法生成extra_data: vps=" << _vps.size() << ", sps=" << _sps.size()
              << ", pps=" << _pps.size() << ", capacity=" << sizeof(hevc.data);
        return nullptr;
    }
    string vps_sps_pps = string("\x00\x00\x00\x01", 4) + _vps + string("\x00\x00\x00\x01", 4) + _sps + string("\x00\x00\x00\x01", 4) + _pps;
    // annexbtomp4 在仅填充配置、没有媒体输出缓冲区时固定返回 0；from_nalu 是库为该场景提供的封装，并会确认参数集已写入 hevc。
    // annexbtomp4 always returns zero when only populating configuration without a media output buffer; from_nalu wraps that use case and verifies parameter sets were stored in hevc.
    if (mpeg4_hevc_from_nalu((const uint8_t *)vps_sps_pps.data(), vps_sps_pps.size(), &hevc) <= 0) {
        WarnL << "生成H265 extra_data时转换参数集失败";
        return nullptr;
    }

    // 固定的 1024 字节缓冲区小于 mpeg4_hevc_t 可保存的参数集；按输入大小分配，并为 HEVC 配置记录字段保留充足空间。
    // A fixed 1024-byte buffer is smaller than the parameter sets held by mpeg4_hevc_t; size it from the input and leave ample room for HEVC record fields.
    std::string extra_data;
    extra_data.resize(vps_sps_pps.size() + 64);
    auto extra_data_size = mpeg4_hevc_decoder_configuration_record_save(&hevc, (uint8_t *)extra_data.data(), extra_data.size());
    if (extra_data_size <= 0) {
        WarnL << "生成H265 extra_data 失败";
        return nullptr;
    }
    extra_data.resize(extra_data_size);
    return std::make_shared<BufferString>(std::move(extra_data));
#else
    WarnL << "请开启MP4相关功能并使能\"ENABLE_MP4\",否则对H265的支持不完善";
    return nullptr;
#endif
}

void H265Track::setExtraData(const uint8_t *data, size_t bytes) {
#ifdef ENABLE_MP4
    struct mpeg4_hevc_t hevc;
    memset(&hevc, 0, sizeof(hevc));
    if (mpeg4_hevc_decoder_configuration_record_load(data, bytes, &hevc) > 0) {
        std::vector<uint8_t> config(bytes * 2);
        int size = mpeg4_hevc_to_nalu(&hevc, config.data(), bytes * 2);
        if (size > 4) {
            splitH264((char *)config.data(), size, 4, [&](const char *ptr, size_t len, size_t prefix) {
                inputFrame_l(std::make_shared<H265FrameNoCacheAble>((char *)ptr, len, 0, 0, prefix));
            });
            update();
        }
    }
#else
    WarnL << "请开启MP4相关功能并使能\"ENABLE_MP4\",否则对H265的支持不完善";
#endif
}

bool H265Track::update() {
    return getHEVCInfo(_vps, _sps, _width, _height, _fps);
}

std::vector<Frame::Ptr> H265Track::getConfigFrames() const {
    if (!ready()) {
        return {};
    }
    return { createConfigFrame<H265Frame>(_vps, 0, getIndex()),
             createConfigFrame<H265Frame>(_sps, 0, getIndex()),
             createConfigFrame<H265Frame>(_pps, 0, getIndex()) };
}

Track::Ptr H265Track::clone() const {
    return std::make_shared<H265Track>(*this);
}

void H265Track::insertConfigFrame(const Frame::Ptr &frame) {
    if (!_vps.empty()) {
        VideoTrack::inputFrame(createConfigFrame<H265Frame>(_vps, frame->dts(), frame->getIndex()));
    }
    if (!_sps.empty()) {
        VideoTrack::inputFrame(createConfigFrame<H265Frame>(_sps, frame->dts(), frame->getIndex()));
    }
    if (!_pps.empty()) {
        VideoTrack::inputFrame(createConfigFrame<H265Frame>(_pps, frame->dts(), frame->getIndex()));
    }
}

class BitReader {
public:
    BitReader(const uint8_t* data, size_t size) : _data(data), _size(size), _bitPos(0) {}

    uint32_t readBits(int n) {
        uint32_t result = 0;
        for (int i = 0; i < n; i++) {
            if (_bitPos >= _size * 8) throw std::runtime_error("Out of range");
            int bytePos = _bitPos / 8;
            int bitOffset = 7 - (_bitPos % 8);
            result = (result << 1) | ((_data[bytePos] >> bitOffset) & 0x01);
            _bitPos++;
        }
        return result;
    }

    void skipBits(int n) {
        _bitPos += n;
        if (_bitPos > _size * 8) throw std::runtime_error("Skip out of range");
    }

private:
    const uint8_t* _data;
    size_t _size;
    size_t _bitPos;
};

struct HevcProfileInfo {
    int profile_id = -1; // profile-id
    int level_id   = -1; // level-id
    int tier_flag  = -1; // tier-flag
};

// 移除 00 00 03 防竞争字节
std::vector<uint8_t> removeEmulationPrevention(const uint8_t *data, size_t size) {
    std::vector<uint8_t> out;
    out.reserve(size);
    for (size_t i = 0; i < size; i++) {
        if (i + 2 < size && data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x03) {
            out.push_back(0x00);
            out.push_back(0x00);
            i += 2; // skip 0x00 0x00 0x03
        } else {
            out.push_back(data[i]);
        }
    }
    return out;
}

// 从 VPS 或 SPS 里提取 profile/level/tier 信息
HevcProfileInfo parse_hevc_profile_tier_level(const uint8_t *nalu, size_t size) {
    // 去掉起始码 (00 00 01 或 00 00 00 01)
    size_t offset = 0;
    if (size > 4 && nalu[0] == 0x00 && nalu[1] == 0x00) {
        if (nalu[2] == 0x01)
            offset = 3;
        else if (nalu[2] == 0x00 && nalu[3] == 0x01)
            offset = 4;
    }

    auto rbsp = removeEmulationPrevention(nalu + offset, size - offset);
    BitReader br(rbsp.data(), rbsp.size());

    // ---- NALU header ----
    br.skipBits(1 + 6 + 6 + 3); // forbidden_zero_bit + nal_unit_type + nuh_layer_id + nuh_temporal_id_plus1

    // VPS 和 SPS 都包含 profile_tier_level()
    // 先解析最少需要的部分

    // vps_video_parameter_set_id 或 sps_video_parameter_set_id (略过)
    br.readBits(4);

    // sps 里还有 sps_max_sub_layers_minus1
    uint32_t max_sub_layers_minus1 = br.readBits(3);
    // temporal_id_nesting_flag
    br.readBits(1);

    // ---- profile_tier_level ----
    HevcProfileInfo info;
    uint32_t profile_space = br.readBits(2); // general_profile_space
    info.tier_flag = br.readBits(1); // general_tier_flag
    info.profile_id = br.readBits(5); // general_profile_idc

    // general_profile_compatibility_flag[32]
    for (int i = 0; i < 32; i++)
        br.readBits(1);

    // general_progressive_source_flag 等 (跳过)
    br.readBits(1); // progressive_source_flag
    br.readBits(1); // interlaced_source_flag
    br.readBits(1); // non_packed_constraint_flag
    br.readBits(1); // frame_only_constraint_flag

    // general_reserved_zero_44bits
    br.skipBits(44);

    // general_level_idc (8 bits)
    info.level_id = br.readBits(8);

    return info;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * h265类型sdp
 * h265 type sdp
 
 * [AUTO-TRANSLATED:4418a7df]
 */
class H265Sdp : public Sdp {
public:
    /**
     * 构造函数
     * @param sps 265 sps,不带0x00000001头
     * @param pps 265 pps,不带0x00000001头
     * @param payload_type  rtp payload type 默认96
     * @param bitrate 比特率
     * Constructor
     * @param sps 265 sps, without 0x00000001 header
     * @param pps 265 pps, without 0x00000001 header
     * @param payload_type  rtp payload type, default 96
     * @param bitrate Bitrate
     
     * [AUTO-TRANSLATED:93f4ec48]
     */
    H265Sdp(const string &strVPS, const string &strSPS, const string &strPPS, int payload_type, int bitrate) : Sdp(90000, payload_type) {
        // 视频通道  [AUTO-TRANSLATED:642ca881]
        // Video channel
        _printer << "m=video 0 RTP/AVP " << payload_type << "\r\n";
        if (bitrate) {
            _printer << "b=AS:" << bitrate << "\r\n";
        }
        _printer << "a=rtpmap:" << payload_type << " " << getCodecName(CodecH265) << "/" << 90000 << "\r\n";

        auto info = parse_hevc_profile_tier_level((uint8_t *)strSPS.data(), strSPS.size());
        _printer << "a=fmtp:" << payload_type << " level-id=" << info.level_id << "; profile-id=" << info.profile_id << "; tier-flag=" << info.tier_flag << "; ";
        _printer << "sprop-vps=";
        _printer << encodeBase64(strVPS) << "; ";
        _printer << "sprop-sps=";
        _printer << encodeBase64(strSPS) << "; ";
        _printer << "sprop-pps=";
        _printer << encodeBase64(strPPS) << "\r\n";
    }

    string getSdp() const override { return _printer; }

private:
    _StrPrinter _printer;
};

Sdp::Ptr H265Track::getSdp(uint8_t payload_type) const {
    return std::make_shared<H265Sdp>(_vps, _sps, _pps, payload_type, getBitRate() >> 10);
}

namespace {

CodecId getCodec() {
    return CodecH265;
}

Track::Ptr getTrackByCodecId(int sample_rate, int channels, int sample_bit) {
    return std::make_shared<H265Track>();
}

Track::Ptr getTrackBySdp(const SdpTrack::Ptr &track) {
    // a=fmtp:96 sprop-sps=QgEBAWAAAAMAsAAAAwAAAwBdoAKAgC0WNrkky/AIAAADAAgAAAMBlQg=; sprop-pps=RAHA8vA8kAA=
    auto map = Parser::parseArgs(track->_fmtp, ";", "=");
    auto vps = decodeBase64(map["sprop-vps"]);
    auto sps = decodeBase64(map["sprop-sps"]);
    auto pps = decodeBase64(map["sprop-pps"]);
    if (sps.empty() || pps.empty()) {
        // 如果sdp里面没有sps/pps,那么可能在后续的rtp里面恢复出sps/pps  [AUTO-TRANSLATED:9300510b]
        // If there is no sps/pps in the sdp, then it may be possible to recover sps/pps from the subsequent rtp
        return std::make_shared<H265Track>();
    }
    return std::make_shared<H265Track>(vps, sps, pps,
                                       prefixSize(vps.data(), vps.size()),
                                       prefixSize(sps.data(), sps.size()),
                                       prefixSize(pps.data(), pps.size()));
}

RtpCodec::Ptr getRtpEncoderByCodecId(uint8_t pt) {
    return std::make_shared<H265RtpEncoder>();
}

RtpCodec::Ptr getRtpDecoderByCodecId() {
    return std::make_shared<H265RtpDecoder>();
}

RtmpCodec::Ptr getRtmpEncoderByTrack(const Track::Ptr &track) {
    return std::make_shared<H265RtmpEncoder>(track);
}

RtmpCodec::Ptr getRtmpDecoderByTrack(const Track::Ptr &track) {
    return std::make_shared<H265RtmpDecoder>(track);
}

Frame::Ptr getFrameFromPtr(const char *data, size_t bytes, uint64_t dts, uint64_t pts) {
    return std::make_shared<H265FrameNoCacheAble>((char *)data, bytes, dts, pts, prefixSize(data, bytes));
}

} // namespace

CodecPlugin h265_plugin = { getCodec,
                            getTrackByCodecId,
                            getTrackBySdp,
                            getRtpEncoderByCodecId,
                            getRtpDecoderByCodecId,
                            getRtmpEncoderByTrack,
                            getRtmpDecoderByTrack,
                            getFrameFromPtr };

}//namespace mediakit
