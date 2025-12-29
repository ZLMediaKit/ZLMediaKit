/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_SVAC_H
#define ZLMEDIAKIT_SVAC_H

#include "Extension/Frame.h"
#include "Extension/Track.h"

namespace mediakit {

/**
 * SVAC编解码器ID
 * SVAC (Surveillance Video and Audio Coding) 是中国国标GB/T 25724定义的音视频编码标准
 * SVAC1: 基于H.264的国标视频编码
 * SVAC2: 新一代国标视频编码（类似H.265）
 */

// 注册SVAC编解码器ID（如果尚未注册）
#ifndef CodecSVAC1
#define CodecSVAC1 CodecId(102)
#endif

#ifndef CodecSVAC2
#define CodecSVAC2 CodecId(103)
#endif

/**
 * SVAC1 Frame (基于H.264)
 */
class SVAC1Frame : public FrameImp {
public:
    using Ptr = std::shared_ptr<SVAC1Frame>;
    
    SVAC1Frame() {
        _codec_id = CodecSVAC1;
    }
    
    bool keyFrame() const override;
    bool configFrame() const override;
    
    /**
     * 获取NALU类型
     */
    int getNaluType() const;
};

/**
 * SVAC1 Track
 */
class SVAC1Track : public VideoTrackImp {
public:
    using Ptr = std::shared_ptr<SVAC1Track>;
    
    SVAC1Track(int width = 1920, int height = 1080, int fps = 25)
        : VideoTrackImp(CodecSVAC1, width, height, fps) {}
    
    bool ready() const override { return _ready; }
    
    Track::Ptr clone() const override {
        return std::make_shared<SVAC1Track>(*this);
    }
    
    Sdp::Ptr getSdp(uint8_t payload_type) const override;
    
    void setSPS(const std::string &sps) { _sps = sps; _ready = !_sps.empty() && !_pps.empty(); }
    void setPPS(const std::string &pps) { _pps = pps; _ready = !_sps.empty() && !_pps.empty(); }
    
    const std::string &getSPS() const { return _sps; }
    const std::string &getPPS() const { return _pps; }

private:
    bool _ready = false;
    std::string _sps;
    std::string _pps;
};

/**
 * SVAC2 Frame (类似H.265)
 */
class SVAC2Frame : public FrameImp {
public:
    using Ptr = std::shared_ptr<SVAC2Frame>;
    
    SVAC2Frame() {
        _codec_id = CodecSVAC2;
    }
    
    bool keyFrame() const override;
    bool configFrame() const override;
    
    /**
     * 获取NALU类型
     */
    int getNaluType() const;
};

/**
 * SVAC2 Track
 */
class SVAC2Track : public VideoTrackImp {
public:
    using Ptr = std::shared_ptr<SVAC2Track>;
    
    SVAC2Track(int width = 1920, int height = 1080, int fps = 25)
        : VideoTrackImp(CodecSVAC2, width, height, fps) {}
    
    bool ready() const override { return _ready; }
    
    Track::Ptr clone() const override {
        return std::make_shared<SVAC2Track>(*this);
    }
    
    Sdp::Ptr getSdp(uint8_t payload_type) const override;
    
    void setVPS(const std::string &vps) { _vps = vps; checkReady(); }
    void setSPS(const std::string &sps) { _sps = sps; checkReady(); }
    void setPPS(const std::string &pps) { _pps = pps; checkReady(); }
    
    const std::string &getVPS() const { return _vps; }
    const std::string &getSPS() const { return _sps; }
    const std::string &getPPS() const { return _pps; }

private:
    void checkReady() {
        _ready = !_vps.empty() && !_sps.empty() && !_pps.empty();
    }
    
    bool _ready = false;
    std::string _vps;
    std::string _sps;
    std::string _pps;
};

/**
 * SVAC音频编解码器（基于G.711/G.722的扩展）
 */
class SVACAudioTrack : public AudioTrackImp {
public:
    using Ptr = std::shared_ptr<SVACAudioTrack>;
    
    SVACAudioTrack(int sample_rate = 8000, int channels = 1)
        : AudioTrackImp(CodecG711A, sample_rate, channels, 16) {}
    
    bool ready() const override { return true; }
    
    Track::Ptr clone() const override {
        return std::make_shared<SVACAudioTrack>(*this);
    }
};

} // namespace mediakit

#endif // ZLMEDIAKIT_SVAC_H
