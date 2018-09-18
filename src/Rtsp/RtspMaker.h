//
// Created by xzl on 2018/9/18.
//

#ifndef ZLMEDIAKIT_RTSPMAKER_H
#define ZLMEDIAKIT_RTSPMAKER_H

#include "Device/base64.h"
#include "Rtsp.h"
#include "RTP/RtpMaker.h"
#include "RTP/RtpMakerH264.h"
#include "RTP/RtpMakerAAC.h"
#include "Player/PlayerBase.h"

using namespace ZL::Player;
using namespace ZL::Rtsp;

namespace ZL{
namespace Rtsp{

class Sdp {
public:
    typedef std::shared_ptr<Sdp> Ptr;
    virtual ~Sdp(){}
    virtual string getSdp() { return "";};
    virtual TrackType getTrackType() { return TrackInvalid;};
    virtual RtpMaker::Ptr createRtpMaker(const RtpMaker::onGetRTP &cb,uint32_t ui32Ssrc, int iMtuSize) { return nullptr;};
};

class SdpTitle : public Sdp{
public:
    SdpTitle(float dur_sec = 0,
             const map<string,string> &header = map<string,string>(),
             int version = 0){
        _printer << "v=" << version << "\r\n";

        if(!header.empty()){
            for (auto &pr : header){
                _printer << pr.first << "=" << pr.second << "\r\n";
            }
        } else {
            _printer << "o=- 1383190487994921 1 IN IP4 0.0.0.0\r\n";
            _printer << "s=RTSP Session, streamed by the ZL\r\n";
            _printer << "i=ZL Live Stream\r\n";
            _printer << "c=IN IP4 0.0.0.0\r\n";
            _printer << "t=0 0\r\n";
        }

        if(dur_sec <= 0){
            _printer << "a=range:npt=0-\r\n";
        }else{
            _printer << "a=range:npt=0-" << dur_sec  << "\r\n";
        }
        _printer << "a=control:*\r\n";
    }
    string getSdp() override {
        return _printer;
    }

private:
    _StrPrinter _printer;
};

class SdpH264 : public Sdp {
public:
    SdpH264(const string &sps,
            const string &pps,
            int sample_rate = 90000,
            int playload_type = 96,
            int track_id = TrackVideo,
            int bitrate = 4000){

        _playload_type = playload_type;
        _sample_rate = sample_rate;
        _track_id = track_id;
        
        //视频通道
        _printer << "m=video 0 RTP/AVP " << playload_type << "\r\n";
        _printer << "b=AS:" << bitrate << "\r\n";
        _printer << "a=rtpmap:" << playload_type << " H264/" << sample_rate << "\r\n";
        _printer << "a=fmtp:" << playload_type << " packetization-mode=1;profile-level-id=";

        char strTemp[100];
        int profile_level_id = 0;
        string strSPS = sps.substr(4);
        string strPPS = pps.substr(4);
        if (strSPS.length() >= 4) { // sanity check
            profile_level_id = (strSPS[1] << 16) | (strSPS[2] << 8) | strSPS[3]; // profile_idc|constraint_setN_flag|level_idc
        }
        memset(strTemp, 0, 100);
        sprintf(strTemp, "%06X", profile_level_id);
        _printer << strTemp;
        _printer << ";sprop-parameter-sets=";
        memset(strTemp, 0, 100);
        av_base64_encode(strTemp, 100, (uint8_t *) strSPS.data(), strSPS.size());
        _printer << strTemp << ",";
        memset(strTemp, 0, 100);
        av_base64_encode(strTemp, 100, (uint8_t *) strPPS.data(), strPPS.size());
        _printer << strTemp << "\r\n";
        _printer << "a=control:trackID=" << track_id << "\r\n";
    }

    string getSdp() override {
        return _printer;
    }

    TrackType getTrackType() override {
        return TrackVideo;
    };

    RtpMaker::Ptr createRtpMaker(const RtpMaker::onGetRTP &cb,uint32_t ui32Ssrc, int iMtuSize) override{
        return std::make_shared<RtpMaker_H264>(cb,ui32Ssrc,iMtuSize,_sample_rate,_playload_type,_track_id * 2);
    };

private:
    _StrPrinter _printer;
    int _playload_type;
    int _sample_rate;
    int _track_id;

};


class SdpAAC : public Sdp {
public:
    SdpAAC(const string &aac_cfg,
           int sample_rate,
           int playload_type = 98,
           int track_id = TrackAudio,
           int bitrate = 128){

        _playload_type = playload_type;
        _sample_rate = sample_rate;
        _track_id = track_id;

        _printer << "m=audio 0 RTP/AVP " << playload_type << "\r\n";
        _printer << "b=AS:" << bitrate << "\r\n";
        _printer << "a=rtpmap:" << playload_type << " MPEG4-GENERIC/" << sample_rate << "\r\n";

        char configStr[32] = {0};
        snprintf(configStr, sizeof(configStr), "%02X%02x", aac_cfg[0], aac_cfg[1]);
        _printer << "a=fmtp:" << playload_type << " streamtype=5;profile-level-id=1;mode=AAC-hbr;"
                 << "sizelength=13;indexlength=3;indexdeltalength=3;config="
                 << configStr << "\r\n";
        _printer << "a=control:trackID=" << track_id << "\r\n";
    }

    string getSdp() override {
        return _printer;
    }

    TrackType getTrackType() override {
        return TrackAudio;
    };

    RtpMaker::Ptr createRtpMaker(const RtpMaker::onGetRTP &cb,uint32_t ui32Ssrc, int iMtuSize) override{
        return std::make_shared<RtpMaker_AAC>(cb,ui32Ssrc,iMtuSize,_sample_rate,_playload_type,_track_id * 2);
    };
private:
    _StrPrinter _printer;
    int _playload_type;
    int _sample_rate;
    int _track_id;
};

class RtspMaker{
public:
    RtspMaker(const RtpMaker::onGetRTP &callback) : _vec_rtp_maker(8){
        _callback = callback;
    }
    
    template<typename First,typename ...Others>
    void addTrack(First && first,Others &&...others){
        addTrack(std::forward<First>(first));
        addTrack(std::forward<Others>(others)...);
    }
    template<typename First>
    void addTrack(First && first){
        _printer << first->getSdp();
        if(first->getTrackType() >= 0){
            auto rtpMaker = first-> createRtpMaker(_callback, 0,1400);
            _vec_rtp_maker[first->getTrackType()] = rtpMaker;
        }
    }

    virtual ~RtspMaker() {};
    
    string getSdp() {
        return _printer;
    }

    bool makeRtp(TrackType type,const char *pcData, int iDataLen, uint32_t uiStamp){
        if(type < 0 || type > _vec_rtp_maker.size()){
            return false;
        }
        auto track = _vec_rtp_maker[type];
        if(!track){
            return false;
        }
        track->makeRtp(pcData,iDataLen,uiStamp);
        return true;
    }

private:
    vector<RtpMaker::Ptr> _vec_rtp_maker;
    _StrPrinter _printer;
    RtpMaker::onGetRTP _callback;

};






}
}



#endif //ZLMEDIAKIT_RTSPMAKER_H
