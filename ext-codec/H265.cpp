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

#ifdef ENABLE_MP4
#include "mpeg4-hevc.h"
#endif

using namespace std;
using namespace toolkit;

namespace mediakit {

// ---- 内部比特流工具（H265） ----
namespace {

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
        while (!eof() && read_bits(1) == 0) z++;
        if (z == 0) return 0;
        if (z >= 32) throw std::runtime_error("exp-golomb overflow");
        return (1u << z) - 1 + read_bits(z);
    }
    int32_t read_se() {
        uint32_t v = read_ue();
        return (v & 1) ? (int32_t)((v + 1) >> 1) : -(int32_t)(v >> 1);
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

} // anonymous namespace

// ---- H265 VPS 解析（只提取帧率用的 timing info） ----
static bool parse_hevc_vps_fps(const uint8_t *data, size_t size, float &fps) {
    // data 为 NALU 原始数据（含 NAL header）
    if (size < 3) return false;
    auto rbsp = h265_rbsp_from_nalu(data, size);
    try {
        H265BS bs(rbsp.data(), rbsp.size());
        // NALU header: forbidden_zero_bit(1) + nal_unit_type(6) + nuh_layer_id(6) + nuh_temporal_id_plus1(3)
        bs.skip_bits(16);
        // vps_video_parameter_set_id(4) + vps_reserved_three_2bits(2) + vps_max_layers_minus1(6)
        bs.skip_bits(4 + 2 + 6);
        uint32_t vps_max_sub_layers_minus1 = bs.read_bits(3);
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
        uint32_t vps_num_layer_sets_minus1 = bs.read_ue();
        for (uint32_t i = 1; i <= vps_num_layer_sets_minus1; i++) {
            for (uint32_t j = 0; j <= vps_max_layer_id; j++) bs.skip_bits(1);
        }
        if (bs.read_bits(1)) { // vps_timing_info_present_flag
            uint32_t vps_num_units_in_tick = bs.read_bits(32);
            uint32_t vps_time_scale        = bs.read_bits(32);
            if (vps_num_units_in_tick > 0) {
                fps = (float)vps_time_scale / (float)vps_num_units_in_tick;
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

// ---- H265 SPS 解析（宽高 + 备用帧率） ----
static bool parse_hevc_sps(const uint8_t *data, size_t size,
                            int &width, int &height, float &fps) {
    if (size < 3) return false;
    auto rbsp = h265_rbsp_from_nalu(data, size);
    try {
        H265BS bs(rbsp.data(), rbsp.size());
        bs.skip_bits(16); // NALU header
        bs.skip_bits(4);  // sps_video_parameter_set_id
        uint32_t sps_max_sub_layers_minus1 = bs.read_bits(3);
        bs.skip_bits(1);  // sps_temporal_id_nesting_flag
        bs.skip_profile_tier_level(true, sps_max_sub_layers_minus1);
        bs.read_ue(); // sps_seq_parameter_set_id
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
            uint32_t crop_left   = bs.read_ue() * sub_width_c;
            uint32_t crop_right  = bs.read_ue() * sub_width_c;
            uint32_t crop_top    = bs.read_ue() * sub_height_c;
            uint32_t crop_bottom = bs.read_ue() * sub_height_c;
            if (crop_left + crop_right > pic_width || crop_top + crop_bottom > pic_height) {
                return false;
            }
            pic_width  -= crop_left + crop_right;
            pic_height -= crop_top + crop_bottom;
        }

        width  = (int)pic_width;
        height = (int)pic_height;

        bs.read_ue(); // bit_depth_luma_minus8
        bs.read_ue(); // bit_depth_chroma_minus8
        uint32_t log2_max_pic_order_cnt_lsb_minus4 = bs.read_ue();

        bool sps_sub_layer_ordering_info_present_flag = bs.read_bits(1) != 0;
        uint32_t start = sps_sub_layer_ordering_info_present_flag ? 0 : sps_max_sub_layers_minus1;
        for (uint32_t i = start; i <= sps_max_sub_layers_minus1; i++) {
            bs.read_ue(); bs.read_ue(); bs.read_ue();
        }
        bs.read_ue(); // log2_min_luma_coding_block_size_minus3
        bs.read_ue(); // log2_diff_max_min_luma_coding_block_size
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
                prev_num_delta_pocs = cnt;
            } else {
                uint32_t num_neg = bs.read_ue();
                uint32_t num_pos = bs.read_ue();
                prev_num_delta_pocs = num_neg + num_pos;
                for (uint32_t j = 0; j < num_neg; j++) { bs.read_ue(); bs.skip_bits(1); }
                for (uint32_t j = 0; j < num_pos; j++) { bs.read_ue(); bs.skip_bits(1); }
            }
        }

        if (bs.read_bits(1)) { // long_term_ref_pics_present_flag
            uint32_t n = bs.read_ue();
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
                if (num_units > 0 && fps <= 0.0f) {
                    fps = (float)time_scale / (float)num_units;
                }
            }
        }
        return width > 0 && height > 0;
    } catch (...) {
        return width > 0 && height > 0;
    }
}

bool getHEVCInfo(const char *vps, size_t vps_len, const char *sps, size_t sps_len,
                 int &iVideoWidth, int &iVideoHeight, float &iVideoFps) {
    iVideoWidth = 0; iVideoHeight = 0; iVideoFps = 0.0f;

    // 先从 VPS 提取帧率
    if (vps_len > 2) {
        parse_hevc_vps_fps((const uint8_t *)vps, vps_len, iVideoFps);
    }

    // 再从 SPS 提取宽高（如果 VPS 没有帧率，SPS VUI 里也可能有）
    if (sps_len <= 2) return false;
    return parse_hevc_sps((const uint8_t *)sps, sps_len, iVideoWidth, iVideoHeight, iVideoFps);
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
    if (_width == 0 && ready()) {
        update();
    }
    return ret;
}

toolkit::Buffer::Ptr H265Track::getExtraData() const {
    CHECK(ready());
#ifdef ENABLE_MP4
    struct mpeg4_hevc_t hevc;
    memset(&hevc, 0, sizeof(hevc));
    string vps_sps_pps = string("\x00\x00\x00\x01", 4) + _vps + string("\x00\x00\x00\x01", 4) + _sps + string("\x00\x00\x00\x01", 4) + _pps;
    h265_annexbtomp4(&hevc, vps_sps_pps.data(), (int) vps_sps_pps.size(), NULL, 0, NULL, NULL);

    std::string extra_data;
    extra_data.resize(1024);
    auto extra_data_size = mpeg4_hevc_decoder_configuration_record_save(&hevc, (uint8_t *)extra_data.data(), extra_data.size());
    if (extra_data_size == -1) {
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

