/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_H264_H
#define ZLMEDIAKIT_H264_H

#include "Frame.h"
#include "Track.h"
#include "Util/base64.h"
#define H264_TYPE(v) ((uint8_t)(v) & 0x1F)
using namespace toolkit;

namespace mediakit{

bool getAVCInfo(const string &strSps,int &iVideoWidth, int &iVideoHeight, float &iVideoFps);
void splitH264(const char *ptr, size_t len, size_t prefix, const std::function<void(const char *, size_t, size_t)> &cb);
size_t prefixSize(const char *ptr, size_t len);

/**
 * 264帧类
 */
class H264Frame : public FrameImp {
public:
    using Ptr = std::shared_ptr<H264Frame>;

    enum {
        NAL_IDR = 5,
        NAL_SEI = 6,
        NAL_SPS = 7,
        NAL_PPS = 8,
        NAL_AUD = 9,
        NAL_B_P = 1,
    };

    bool keyFrame() const override;
    bool configFrame() const override;

protected:
    friend class FrameImp;
    friend class ResourcePool_l<H264Frame>;
    H264Frame();
};

/**
 * 防止内存拷贝的H264类
 * 用户可以通过该类型快速把一个指针无拷贝的包装成Frame类
 * 该类型在DevChannel中有使用
 */
class H264FrameNoCacheAble : public FrameFromPtr {
public:
    using Ptr = std::shared_ptr<H264FrameNoCacheAble>;

    H264FrameNoCacheAble(char *ptr,size_t size,uint32_t dts , uint32_t pts ,size_t prefix_size = 4);
    bool keyFrame() const override;
    bool configFrame() const override;
};

/**
 * 264视频通道
 */
class H264Track : public VideoTrack{
public:
    using Ptr = std::shared_ptr<H264Track>;

    /**
     * 不指定sps pps构造h264类型的媒体
     * 在随后的inputFrame中获取sps pps
     */
    H264Track() = default;

    /**
     * 构造h264类型的媒体
     * @param sps sps帧数据
     * @param pps pps帧数据
     * @param sps_prefix_len 264头长度，可以为3个或4个字节，一般为0x00 00 00 01
     * @param pps_prefix_len 264头长度，可以为3个或4个字节，一般为0x00 00 00 01
     */
    H264Track(const string &sps,const string &pps,int sps_prefix_len = 4,int pps_prefix_len = 4);

    /**
     * 构造h264类型的媒体
     * @param sps sps帧
     * @param pps pps帧
     */
    H264Track(const Frame::Ptr &sps,const Frame::Ptr &pps);

    /**
     * 返回不带0x00 00 00 01头的sps/pps
     */
    const string &getSps() const;
    const string &getPps() const;

    bool ready() override;
    CodecId getCodecId() const override;
    int getVideoHeight() const override;
    int getVideoWidth() const override;
    float getVideoFps() const override;
    void inputFrame(const Frame::Ptr &frame) override;

private:
    void onReady();
    Sdp::Ptr getSdp() override;
    Track::Ptr clone() override;
    void inputFrame_l(const Frame::Ptr &frame);
    void insertConfigFrame(const Frame::Ptr &frame);

private:
    bool _is_idr = false;
    int _width = 0;
    int _height = 0;
    float _fps = 0;
    string _sps;
    string _pps;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_H264_H