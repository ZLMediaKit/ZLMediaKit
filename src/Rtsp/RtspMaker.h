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

/**
 * sdp基类
 */
class Sdp {
public:
    typedef std::shared_ptr<Sdp> Ptr;
    virtual ~Sdp(){}
    /**
     * 获取sdp字符串
     * @return
     */
    virtual string getSdp() const { return "";};

    /**
     * 获取track类型
     * @return
     */
    virtual TrackType getTrackType() const { return TrackInvalid;};

    /**
     * 获取rtp生成器
     * @param cb 回调lambad
     * @param ui32Ssrc rtp ssrc
     * @param iMtuSize rtp mtu
     * @return
     */
    virtual RtpMaker::Ptr createRtpMaker(const RtpMaker::onGetRTP &cb,uint32_t ui32Ssrc, int iMtuSize) const{ return nullptr;};
};

/**
 * sdp中除音视频外的其他描述部分
 */
class SdpTitle : public Sdp{
public:

    /**
     * 构造title类型sdp
     * @param dur_sec rtsp点播时长，0代表直播，单位秒
     * @param header 自定义sdp描述
     * @param version sdp版本
     */
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
    string getSdp() const override {
        return _printer;
    }

private:
    _StrPrinter _printer;
};

/**
 * h264类型sdp
 */
class SdpH264 : public Sdp {
public:

    /**
     *
     * @param sps 264 sps,带0x00000001头
     * @param pps 264 pps,带0x00000001头
     * @param sample_rate 时间戳采样率，视频默认90000
     * @param playload_type rtp playload type 默认96
     * @param track_id trackID 默认为TrackVideo
     * @param bitrate 比特率
     */
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

    string getSdp() const override {
        return _printer;
    }

    TrackType getTrackType() const override {
        return TrackVideo;
    };

    RtpMaker::Ptr createRtpMaker(const RtpMaker::onGetRTP &cb,uint32_t ui32Ssrc, int iMtuSize) const override{
        return std::make_shared<RtpMaker_H264>(cb,ui32Ssrc,iMtuSize,_sample_rate,_playload_type,_track_id * 2);
    };

private:
    _StrPrinter _printer;
    int _playload_type;
    int _sample_rate;
    int _track_id;

};


/**
 * aac类型SDP
 */
class SdpAAC : public Sdp {
public:

    /**
     * 构造aac sdp
     * @param aac_cfg aac两个字节的配置描述
     * @param sample_rate 音频采样率
     * @param playload_type rtp playload type 默认96
     * @param track_id trackID 默认为TrackVideo
     * @param bitrate 比特率
     */
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

    string getSdp() const override {
        return _printer;
    }

    TrackType getTrackType() const override {
        return TrackAudio;
    };

    RtpMaker::Ptr createRtpMaker(const RtpMaker::onGetRTP &cb,uint32_t ui32Ssrc, int iMtuSize) const override{
        return std::make_shared<RtpMaker_AAC>(cb,ui32Ssrc,iMtuSize,_sample_rate,_playload_type,_track_id * 2);
    };
private:
    _StrPrinter _printer;
    int _playload_type;
    int _sample_rate;
    int _track_id;
};

/**
 * rtsp生成器
 */
class RtspMaker{
public:
    /**
     * 构成函数
     * @param callback rtp回调lambad
     */
    RtspMaker(const RtpMaker::onGetRTP &callback) : _vec_rtp_maker(TrackMax){
        _callback = callback;
    }

    /**
     * 添加音视频track
     * @param sdp 媒体描述
     * @param ssrc 媒体rtp ssrc
     * @param mtu 媒体rtp mtu
     * @return 成功与否
     */
    bool addTrack(const Sdp & sdp,uint32_t ssrc = 0,int mtu = 1400){
        auto type = sdp.getTrackType();
        if(type < 0 || type >= _vec_rtp_maker.size()){
            return false;
        }

        if(_vec_rtp_maker[type]){
            return false;
        }

        if(ssrc == 0){
            ssrc = ((uint64_t) &sdp) & 0xFFFFFFFF;
        }

        auto rtpMaker = sdp.createRtpMaker(_callback, ssrc,mtu);
        _vec_rtp_maker[sdp.getTrackType()] = rtpMaker;
        _printer << sdp.getSdp();
        return true;
    }

    virtual ~RtspMaker() {};

    /**
     * 获取完整的SDP字符串
     * @return SDP字符串
     */
    string getSdp() {
        return _printer;
    }


    /**
     * 打包RTP数据包
     * @param type 媒体类型
     * @param pcData 媒体数据
     * @param iDataLen 媒体数据长度
     * @param uiStamp  媒体时间戳，单位毫秒
     * @return 是否成功
     */
    bool makeRtp(TrackType type,const char *pcData, int iDataLen, uint32_t uiStamp){
        if(type < 0 || type >= _vec_rtp_maker.size()){
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
