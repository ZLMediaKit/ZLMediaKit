//
// Created by xzl on 2018/10/18.
//

#include "RtpCodec.h"

RtpEncoder::RtpEncoder(uint32_t ui32Ssrc,
                       uint32_t ui32MtuSize,
                       uint32_t ui32SampleRate,
                       uint8_t ui8PlayloadType,
                       uint8_t ui8Interleaved) :
        RtpInfo(ui32Ssrc,
                ui32MtuSize,
                ui32SampleRate,
                ui8PlayloadType,
                ui8Interleaved) {

}
