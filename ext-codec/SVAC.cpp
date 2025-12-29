/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "SVAC.h"
#include "Rtsp/Rtsp.h"
#include "Util/base64.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

//////////////////////////////////////////////////////////////////
// SVAC1 实现
//////////////////////////////////////////////////////////////////

// SVAC1 NALU类型（类似H.264）
enum SVAC1NaluType {
    SVAC1_NAL_SLICE = 1,
    SVAC1_NAL_SLICE_DPA = 2,
    SVAC1_NAL_SLICE_DPB = 3,
    SVAC1_NAL_SLICE_DPC = 4,
    SVAC1_NAL_SLICE_IDR = 5,
    SVAC1_NAL_SEI = 6,
    SVAC1_NAL_SPS = 7,
    SVAC1_NAL_PPS = 8,
    SVAC1_NAL_AUD = 9,
    // SVAC特有类型
    SVAC1_NAL_SVA_EXT = 14,
};

int SVAC1Frame::getNaluType() const {
    if (size() < 1) return 0;
    return data()[0] & 0x1F;
}

bool SVAC1Frame::keyFrame() const {
    int type = getNaluType();
    return type == SVAC1_NAL_SLICE_IDR;
}

bool SVAC1Frame::configFrame() const {
    int type = getNaluType();
    return type == SVAC1_NAL_SPS || type == SVAC1_NAL_PPS;
}

Sdp::Ptr SVAC1Track::getSdp(uint8_t payload_type) const {
    class SVAC1Sdp : public Sdp {
    public:
        SVAC1Sdp(const string &sps, const string &pps, int width, int height, 
                 int fps, uint8_t payload_type)
            : Sdp(90000, payload_type)
            , _sps(sps), _pps(pps)
            , _width(width), _height(height), _fps(fps) {}
        
        string getSdp() const override {
            _StrPrinter printer;
            printer << "m=video 0 RTP/AVP " << (int)getPayloadType() << "\r\n";
            printer << "a=rtpmap:" << (int)getPayloadType() << " SVAC1/90000\r\n";
            
            if (!_sps.empty() || !_pps.empty()) {
                printer << "a=fmtp:" << (int)getPayloadType();
                // 类似H.264的profile-level-id和sprop-parameter-sets
                printer << " profile-level-id=42001f";
                if (!_sps.empty()) {
                    printer << ";sprop-sps=" << encodeBase64(_sps);
                }
                if (!_pps.empty()) {
                    printer << ";sprop-pps=" << encodeBase64(_pps);
                }
                printer << "\r\n";
            }
            
            printer << "a=control:trackID=" << (int)TrackVideo << "\r\n";
            return std::move(printer);
        }
        
    private:
        string _sps;
        string _pps;
        int _width;
        int _height;
        int _fps;
    };
    
    return make_shared<SVAC1Sdp>(_sps, _pps, getVideoWidth(), getVideoHeight(),
                                  (int)getVideoFps(), payload_type);
}

//////////////////////////////////////////////////////////////////
// SVAC2 实现
//////////////////////////////////////////////////////////////////

// SVAC2 NALU类型（类似H.265）
enum SVAC2NaluType {
    SVAC2_NAL_TRAIL_N = 0,
    SVAC2_NAL_TRAIL_R = 1,
    SVAC2_NAL_BLA_W_LP = 16,
    SVAC2_NAL_BLA_W_RADL = 17,
    SVAC2_NAL_BLA_N_LP = 18,
    SVAC2_NAL_IDR_W_RADL = 19,
    SVAC2_NAL_IDR_N_LP = 20,
    SVAC2_NAL_CRA_NUT = 21,
    SVAC2_NAL_VPS = 32,
    SVAC2_NAL_SPS = 33,
    SVAC2_NAL_PPS = 34,
    SVAC2_NAL_AUD = 35,
    SVAC2_NAL_SEI_PREFIX = 39,
    SVAC2_NAL_SEI_SUFFIX = 40,
    // SVAC2特有类型
    SVAC2_NAL_SVA_EXT = 48,
};

int SVAC2Frame::getNaluType() const {
    if (size() < 2) return 0;
    return (data()[0] >> 1) & 0x3F;
}

bool SVAC2Frame::keyFrame() const {
    int type = getNaluType();
    return type >= SVAC2_NAL_BLA_W_LP && type <= SVAC2_NAL_CRA_NUT;
}

bool SVAC2Frame::configFrame() const {
    int type = getNaluType();
    return type == SVAC2_NAL_VPS || type == SVAC2_NAL_SPS || type == SVAC2_NAL_PPS;
}

Sdp::Ptr SVAC2Track::getSdp(uint8_t payload_type) const {
    class SVAC2Sdp : public Sdp {
    public:
        SVAC2Sdp(const string &vps, const string &sps, const string &pps,
                 int width, int height, int fps, uint8_t payload_type)
            : Sdp(90000, payload_type)
            , _vps(vps), _sps(sps), _pps(pps)
            , _width(width), _height(height), _fps(fps) {}
        
        string getSdp() const override {
            _StrPrinter printer;
            printer << "m=video 0 RTP/AVP " << (int)getPayloadType() << "\r\n";
            printer << "a=rtpmap:" << (int)getPayloadType() << " SVAC2/90000\r\n";
            
            if (!_vps.empty() || !_sps.empty() || !_pps.empty()) {
                printer << "a=fmtp:" << (int)getPayloadType();
                // 类似H.265的sprop-vps/sps/pps
                if (!_vps.empty()) {
                    printer << " sprop-vps=" << encodeBase64(_vps);
                }
                if (!_sps.empty()) {
                    printer << ";sprop-sps=" << encodeBase64(_sps);
                }
                if (!_pps.empty()) {
                    printer << ";sprop-pps=" << encodeBase64(_pps);
                }
                printer << "\r\n";
            }
            
            printer << "a=control:trackID=" << (int)TrackVideo << "\r\n";
            return std::move(printer);
        }
        
    private:
        string _vps;
        string _sps;
        string _pps;
        int _width;
        int _height;
        int _fps;
    };
    
    return make_shared<SVAC2Sdp>(_vps, _sps, _pps, getVideoWidth(), getVideoHeight(),
                                  (int)getVideoFps(), payload_type);
}

} // namespace mediakit
