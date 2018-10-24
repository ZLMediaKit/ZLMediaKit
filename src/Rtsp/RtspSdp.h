//
// Created by xzl on 2018/9/18.
//

#ifndef ZLMEDIAKIT_RTSPSDP_H
#define ZLMEDIAKIT_RTSPSDP_H

#include "RTP/H264RtpCodec.h"
#include "RTP/AACRtpCodec.h"
#include "Util/base64.h"
#include "Player/Track.h"

namespace ZL {
namespace Rtsp {

/**
* sdp基类
*/
class Sdp :  public FrameRingInterface , public RtpRingInterface , public CodecInfo{
public:
    typedef std::shared_ptr<Sdp> Ptr;

    /**
     * 构造sdp
     * @param sample_rate 采样率
     * @param playload_type pt类型
     */
    Sdp(uint32_t sample_rate, uint8_t playload_type){
        _sample_rate = sample_rate;
        _playload_type = playload_type;
    }

    virtual ~Sdp(){}

    /**
     * 获取sdp字符串
     * @return
     */
    virtual string getSdp() const  = 0;

    /**
     * 返回音频或视频类型
     * @return
     */
    TrackType getTrackType() const override {
        return TrackInvalid;
    }

    /**
     * 返回编码器id
     * @return
     */
    CodecId getCodecId() const override{
        return CodecInvalid;
    }

    /**
     * 获取帧环形缓存
     * @return
     */
    FrameRingInterface::RingType::Ptr getFrameRing() const override {
        return _encoder->getFrameRing();
    }

    /**
     * 获取rtp环形缓存
     * @return
     */
    RtpRingInterface::RingType::Ptr getRtpRing() const override{
        return _encoder->getRtpRing();
    }

    /**
     * 输入帧数据，驱动rtp打包
     * @param frame 帧数据
     */
    void inputFrame(const Frame::Ptr &frame) override{
        _encoder->inputFrame(frame);
    }

    /**
     * 也可以在外部打包rtp后直接输入rtp
     * @param rtp rtp数据包
     * @param key_pos 是否为关键帧第一个rtp包
     */
    bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos) override{
        return _encoder->inputRtp(rtp,key_pos);
    }

    /**
     * 替换帧环形缓存，目的是多个rtp打包器共用一个环形缓存
     * @param ring
     */
    void setFrameRing(const FrameRingInterface::RingType::Ptr &ring) override{
        _encoder->setFrameRing(ring);
    }

    /**
     * 替换帧环形缓存，目的是多个rtp打包器共用一个环形缓存
     * @param ring
     */
    void setRtpRing(const RtpRingInterface::RingType::Ptr &ring) override{
        _encoder->setRtpRing(ring);
    }

    /**
     * 创建Rtp打包器
     * @param ssrc 打包器ssrc，可以为0
     * @param mtu mtu大小，一般小于1500字节，推荐1400
     */
    virtual void createRtpEncoder(uint32_t ssrc, int mtu);
private:
    RtpCodec::Ptr _encoder;
    uint8_t _playload_type;
    uint32_t _sample_rate;
};

/**
* sdp中除音视频外的其他描述部分
*/
class TitleSdp : public Sdp{
public:

    /**
     * 构造title类型sdp
     * @param dur_sec rtsp点播时长，0代表直播，单位秒
     * @param header 自定义sdp描述
     * @param version sdp版本
     */
    TitleSdp(float dur_sec = 0,
             const map<string,string> &header = map<string,string>(),
             int version = 0) : Sdp(0,0){
        _printer << "v=" << version << "\r\n";

        if(!header.empty()){
            for (auto &pr : header){
                _printer << pr.first << "=" << pr.second << "\r\n";
            }
        } else {
            _printer << "o=- 1383190487994921 1 IN IP4 0.0.0.0\r\n";
            _printer << "s=RTSP Session, streamed by the ZLMediaKit\r\n";
            _printer << "i=ZLMediaKit Live Stream\r\n";
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
class H264Sdp : public Sdp {
public:

    /**
     *
     * @param sps 264 sps,不带0x00000001头
     * @param pps 264 pps,不带0x00000001头
     * @param playload_type  rtp playload type 默认96
     * @param bitrate 比特率
     */
    H264Sdp(const string &strSPS,
            const string &strPPS,
            int playload_type = 96,
            int bitrate = 4000) : Sdp(90000,playload_type) {
        //视频通道
        _printer << "m=video 0 RTP/AVP " << playload_type << "\r\n";
        _printer << "b=AS:" << bitrate << "\r\n";
        _printer << "a=rtpmap:" << playload_type << " H264/" << 90000 << "\r\n";
        _printer << "a=fmtp:" << playload_type << " packetization-mode=1;profile-level-id=";

        char strTemp[100];
        int profile_level_id = 0;
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
        _printer << "a=control:trackID=" << getTrackType() << "\r\n";
    }

    string getSdp() const override {
        return _printer;
    }

    TrackType getTrackType() const override {
        return TrackVideo;
    }

    CodecId getCodecId() const override {
        return CodecH264;
    }
private:
    _StrPrinter _printer;
};


/**
* aac类型SDP
*/
class AACSdp : public Sdp {
public:

    /**
     *
     * @param aac_cfg aac两个字节的配置描述
     * @param sample_rate 音频采样率
     * @param playload_type rtp playload type 默认98
     * @param bitrate 比特率
     */
    AACSdp(const string &aac_cfg,
           int sample_rate,
           int playload_type = 98,
           int bitrate = 128) : Sdp(sample_rate,playload_type){
        _printer << "m=audio 0 RTP/AVP " << playload_type << "\r\n";
        _printer << "b=AS:" << bitrate << "\r\n";
        _printer << "a=rtpmap:" << playload_type << " MPEG4-GENERIC/" << sample_rate << "\r\n";

        char configStr[32] = {0};
        snprintf(configStr, sizeof(configStr), "%02X%02x", aac_cfg[0], aac_cfg[1]);
        _printer << "a=fmtp:" << playload_type << " streamtype=5;profile-level-id=1;mode=AAC-hbr;"
                 << "sizelength=13;indexlength=3;indexdeltalength=3;config="
                 << configStr << "\r\n";
        _printer << "a=control:trackID=" << getTrackType() << "\r\n";
    }

    string getSdp() const override {
        return _printer;
    }

    TrackType getTrackType() const override {
        return TrackAudio;
    };
    CodecId getCodecId() const override {
        return CodecAAC;
    }
private:
    _StrPrinter _printer;
};


} /* namespace Rtsp */
} /* namespace ZL */



#endif //ZLMEDIAKIT_RTSPSDP_H
