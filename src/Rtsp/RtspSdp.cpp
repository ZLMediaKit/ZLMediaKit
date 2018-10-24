#include "RtspSdp.h"
#include "Common/Factory.h"

namespace mediakit{

void Sdp::createRtpEncoder(uint32_t ssrc, int mtu) {
    _encoder = Factory::getRtpEncoderById(getCodecId(),
                                          ssrc,
                                          mtu,
                                          _sample_rate,
                                          _playload_type,
                                          getTrackType() * 2);
}

}


