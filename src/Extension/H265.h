/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_H265_H
#define ZLMEDIAKIT_H265_H

#include "Frame.h"
#include "Track.h"
#include "Util/base64.h"
#include "H264.h"

#define H265_TYPE(v) (((uint8_t)(v) >> 1) & 0x3f)

namespace mediakit {

bool getHEVCInfo(const std::string &strVps, const std::string &strSps, int &iVideoWidth, int &iVideoHeight, float &iVideoFps);

template<typename Parent>
class H265FrameHelper : public Parent{
public:
    friend class FrameImp;
    friend class toolkit::ResourcePool_l<H265FrameHelper>;
    using Ptr = std::shared_ptr<H265FrameHelper>;

    enum {
        NAL_TRAIL_N = 0,
        NAL_TRAIL_R = 1,
        NAL_TSA_N = 2,
        NAL_TSA_R = 3,
        NAL_STSA_N = 4,
        NAL_STSA_R = 5,
        NAL_RADL_N = 6,
        NAL_RADL_R = 7,
        NAL_RASL_N = 8,
        NAL_RASL_R = 9,
        NAL_BLA_W_LP = 16,
        NAL_BLA_W_RADL = 17,
        NAL_BLA_N_LP = 18,
        NAL_IDR_W_RADL = 19,
        NAL_IDR_N_LP = 20,
        NAL_CRA_NUT = 21,
        NAL_RSV_IRAP_VCL22 = 22,
        NAL_RSV_IRAP_VCL23 = 23,

        NAL_VPS = 32,
        NAL_SPS = 33,
        NAL_PPS = 34,
        NAL_AUD = 35,
        NAL_EOS_NUT = 36,
        NAL_EOB_NUT = 37,
        NAL_FD_NUT = 38,
        NAL_SEI_PREFIX = 39,
        NAL_SEI_SUFFIX = 40,
    };

    template<typename ...ARGS>
    H265FrameHelper(ARGS &&...args): Parent(std::forward<ARGS>(args)...) {
        this->_codec_id = CodecH265;
    }

    ~H265FrameHelper() override = default;

    bool keyFrame() const override {
        auto nal_ptr = (uint8_t *) this->data() + this->prefixSize();
        auto type = H265_TYPE(*nal_ptr);
        // 参考自FFmpeg: IRAP VCL NAL unit types span the range
        // [BLA_W_LP (16), RSV_IRAP_VCL23 (23)].
        return (type >= NAL_BLA_W_LP && type <= NAL_RSV_IRAP_VCL23) && decodeAble() ;
    }

    bool configFrame() const override {
        auto nal_ptr = (uint8_t *) this->data() + this->prefixSize();
        switch (H265_TYPE(*nal_ptr)) {
            case NAL_VPS:
            case NAL_SPS:
            case NAL_PPS : return true;
            default : return false;
        }
    }

    bool dropAble() const override {
        auto nal_ptr = (uint8_t *) this->data() + this->prefixSize();
        switch (H265_TYPE(*nal_ptr)) {
            case NAL_AUD:
            case NAL_SEI_SUFFIX:
            case NAL_SEI_PREFIX: return true;
            default: return false;
        }
    }

    bool decodeAble() const override {
        auto nal_ptr = (uint8_t *) this->data() + this->prefixSize();
        auto type = H265_TYPE(*nal_ptr);
        //多slice情况下, first_slice_segment_in_pic_flag 表示其为一帧的开始
        return type >= NAL_TRAIL_N && type <= NAL_RSV_IRAP_VCL23 && (nal_ptr[2] & 0x80);
    }
};

/**
 * 265帧类
 */
using H265Frame = H265FrameHelper<FrameImp>;

/**
 * 防止内存拷贝的H265类
 * 用户可以通过该类型快速把一个指针无拷贝的包装成Frame类
 */
using H265FrameNoCacheAble = H265FrameHelper<FrameFromPtr>;

/**
* 265视频通道
*/
class H265Track : public VideoTrack {
public:
    using Ptr = std::shared_ptr<H265Track>;

    /**
     * 不指定sps pps构造h265类型的媒体
     * 在随后的inputFrame中获取sps pps
     */
    H265Track() = default;

    /**
     * 构造h265类型的媒体
     * @param vps vps帧数据
     * @param sps sps帧数据
     * @param pps pps帧数据
     * @param vps_prefix_len 265头长度，可以为3个或4个字节，一般为0x00 00 00 01
     * @param sps_prefix_len 265头长度，可以为3个或4个字节，一般为0x00 00 00 01
     * @param pps_prefix_len 265头长度，可以为3个或4个字节，一般为0x00 00 00 01
     */
    H265Track(const std::string &vps,const std::string &sps, const std::string &pps,int vps_prefix_len = 4, int sps_prefix_len = 4, int pps_prefix_len = 4);

    /**
     * 返回不带0x00 00 00 01头的vps/sps/pps
     */
    const std::string &getVps() const;
    const std::string &getSps() const;
    const std::string &getPps() const;

    bool ready() override;
    CodecId getCodecId() const override;
    int getVideoWidth() const override;
    int getVideoHeight() const override;
    float getVideoFps() const override;
    bool inputFrame(const Frame::Ptr &frame) override;

private:
    void onReady();
    Sdp::Ptr getSdp() override;
    Track::Ptr clone() override;
    bool inputFrame_l(const Frame::Ptr &frame);
    void insertConfigFrame(const Frame::Ptr &frame);

private:
    bool _is_idr = false;
    int _width = 0;
    int _height = 0;
    float _fps = 0;
    std::string _vps;
    std::string _sps;
    std::string _pps;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_H265_H