//
// Created by xzl on 2018/10/21.
//

#include "Track.h"
#include "Util/util.h"
#include "Util/base64.h"

using namespace ZL::Util;

Track::Ptr Track::getTrackBySdp(const string &sdp) {
    if (strcasestr(sdp.data(), "mpeg4-generic") != nullptr) {
        string aac_cfg_str = FindField(sdp.c_str(), "config=", "\r\n");
        if (aac_cfg_str.size() != 4) {
            aac_cfg_str = FindField(sdp.c_str(), "config=", ";");
        }
        if (aac_cfg_str.size() != 4) {
            return nullptr;
        }
        string aac_cfg;

        unsigned int cfg1;
        sscanf(aac_cfg_str.substr(0, 2).c_str(), "%02X", &cfg1);
        cfg1 &= 0x00FF;
        aac_cfg.push_back(cfg1);

        unsigned int cfg2;
        sscanf(aac_cfg_str.substr(2, 2).c_str(), "%02X", &cfg2);
        cfg2 &= 0x00FF;
        aac_cfg.push_back(cfg2);

        return std::make_shared<AACTrack>(aac_cfg);
    }

    if (strcasestr(sdp.data(), "h264") != nullptr) {
        string sps_pps = FindField(sdp.c_str(), "sprop-parameter-sets=", "\r\n");
        if(sps_pps.empty()){
            return std::make_shared<H264Track>();
        }
        string base64_SPS = FindField(sps_pps.c_str(), NULL, ",");
        string base64_PPS = FindField(sps_pps.c_str(), ",", NULL);
        if(base64_PPS.back() == ';'){
            base64_PPS.pop_back();
        }

        auto sps = decodeBase64(base64_SPS);
        auto pps = decodeBase64(base64_PPS);
        return std::make_shared<H264Track>(sps,pps,0,0);
    }


    return nullptr;
}
