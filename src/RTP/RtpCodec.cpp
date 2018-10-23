//
// Created by xzl on 2018/10/18.
//

#include "RtpCodec.h"
#include "AACRtpCodec.h"
#include "H264RtpCodec.h"

RtpCodec::Ptr RtpCodec::getRtpCodecById(CodecId codecId,
                                        uint32_t ui32Ssrc,
                                        uint32_t ui32MtuSize,
                                        uint32_t ui32SampleRate,
                                        uint8_t ui8PlayloadType,
                                        uint8_t ui8Interleaved) {
    switch (codecId){
        case CodecH264:
            return std::make_shared<H264RtpEncoder>(ui32Ssrc,ui32MtuSize,ui32SampleRate,ui8PlayloadType,ui8Interleaved);
        case CodecAAC:
            return std::make_shared<AACRtpEncoder>(ui32Ssrc,ui32MtuSize,ui32SampleRate,ui8PlayloadType,ui8Interleaved);
        default:
            return nullptr;
    }
}
