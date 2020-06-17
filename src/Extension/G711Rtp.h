/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_G711RTPCODEC_H
#define ZLMEDIAKIT_G711RTPCODEC_H
#include "Rtsp/RtpCodec.h"
#include "Extension/G711.h"
namespace mediakit{

/**
 * rtp转G711类
 */
class G711RtpDecoder : public RtpCodec , public ResourcePoolHelper<G711Frame> {
public:
    typedef std::shared_ptr<G711RtpDecoder> Ptr;

    G711RtpDecoder(const Track::Ptr &track);
    ~G711RtpDecoder() {}

    /**
     * 输入rtp并解码
     * @param rtp rtp数据包
     * @param key_pos 此参数内部强制转换为false,请忽略之
     */
    bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos = false) override;

    CodecId getCodecId() const override{
        return _codecid;
    }

protected:
    G711RtpDecoder() {}

private:
    void onGetG711(const G711Frame::Ptr &frame);
    G711Frame::Ptr obtainFrame();

private:
    G711Frame::Ptr _frame;
    CodecId _codecid;
};

/**
 * g711 转rtp类
 */
class G711RtpEncoder : public G711RtpDecoder , public RtpInfo {
public:
    typedef std::shared_ptr<G711RtpEncoder> Ptr;

    /**
     * @param ui32Ssrc ssrc
     * @param ui32MtuSize mtu 大小
     * @param ui32SampleRate 采样率
     * @param ui8PayloadType pt类型
     * @param ui8Interleaved rtsp interleaved 值
     */
    G711RtpEncoder(uint32_t ui32Ssrc,
                   uint32_t ui32MtuSize,
                   uint32_t ui32SampleRate,
                   uint8_t ui8PayloadType = 0,
                   uint8_t ui8Interleaved = TrackAudio * 2);
    ~G711RtpEncoder() {}

    /**
     * @param frame g711数据
     */
    void inputFrame(const Frame::Ptr &frame) override;
private:
    void makeG711Rtp(const void *pData, unsigned int uiLen, bool bMark, uint32_t uiStamp);
};

}//namespace mediakit

#endif //ZLMEDIAKIT_G711RTPCODEC_H
