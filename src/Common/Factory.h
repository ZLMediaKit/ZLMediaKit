//
// Created by xzl on 2018/10/24.
//

#ifndef ZLMEDIAKIT_FACTORY_H
#define ZLMEDIAKIT_FACTORY_H

#include <string>
#include "Player/Track.h"
#include "Rtsp/RtspSdp.h"

using namespace std;
using namespace ZL::Rtsp;

class Factory {
public:
    /**
     * 根据sdp生成Track对象
     */
    static Track::Ptr getTrackBySdp(const string &sdp);

    /**
    * 根据Track生成SDP对象
    * @param track 媒体信息
    * @return 返回sdp对象
    */
    static Sdp::Ptr getSdpByTrack(const Track::Ptr &track);


    /**
     * 根据CodecId生成Rtp打包器
     * @param codecId
     * @param ui32Ssrc
     * @param ui32MtuSize
     * @param ui32SampleRate
     * @param ui8PlayloadType
     * @param ui8Interleaved
     * @return
     */
    static RtpCodec::Ptr getRtpEncoderById(CodecId codecId,
                                           uint32_t ui32Ssrc,
                                           uint32_t ui32MtuSize,
                                           uint32_t ui32SampleRate,
                                           uint8_t ui8PlayloadType,
                                           uint8_t ui8Interleaved);

    /**
     * 根据CodecId生成Rtp解包器
     * @param codecId
     * @param ui32SampleRate
     * @return
     */
    static RtpCodec::Ptr getRtpDecoderById(CodecId codecId, uint32_t ui32SampleRate);
};


#endif //ZLMEDIAKIT_FACTORY_H
