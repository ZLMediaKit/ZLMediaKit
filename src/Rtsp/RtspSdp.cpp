#include "RtspSdp.h"
#include "Common/Factory.h"

void Sdp::createRtpEncoder(uint32_t ssrc, int mtu) {
    _encoder = Factory::getRtpEncoderById(getCodecId(),
                                          ssrc,
                                          mtu,
                                          _sample_rate,
                                          _playload_type,
                                          getTrackType() * 2);
}



