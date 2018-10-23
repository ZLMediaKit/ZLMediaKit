//
// Created by xzl on 2018/9/18.
//

#ifndef ZLMEDIAKIT_RTSPMAKER_H
#define ZLMEDIAKIT_RTSPMAKER_H

#include "RTP/H264RtpCodec.h"
#include "RTP/AACRtpCodec.h"
#include "Util/base64.h"

namespace ZL{
namespace Rtsp{

/**
* sdp基类
*/
class Sdp : public TrackFormat , public RtpRingInterface{
public:
    typedef std::shared_ptr<Sdp> Ptr;
    Sdp(){}
    virtual ~Sdp(){}
    /**
     * 获取sdp字符串
     * @return
     */
    virtual string getSdp() const  = 0;

    TrackType getTrackType() const override {
        return TrackInvalid;
    }

    CodecId getCodecId() const override{
        return CodecInvalid;
    }

    FrameRingInterface::RingType::Ptr getFrameRing() const override {
        return _encoder->getFrameRing();
    }

    RtpRingInterface::RingType::Ptr getRtpRing() const override{
        return _encoder->getRtpRing();
    }

    void inputFrame(const Frame::Ptr &frame,bool key_pos) override{
        _encoder->inputFrame(frame,key_pos);
    }

    void inputRtp(const RtpPacket::Ptr &rtp, bool key_pos) override{
        _encoder->inputRtp(rtp,key_pos);
    }

    virtual void createRtpEncoder(uint32_t ssrc, int mtu) = 0;

    void setFrameRing(const FrameRingInterface::RingType::Ptr &ring) override{
        if(_encoder){
            _encoder->setFrameRing(ring);
        }
    }
    void setRtpRing(const RtpRingInterface::RingType::Ptr &ring) override{
        if(_encoder){
            _encoder->setRtpRing(ring);
        }
    }

protected:
    RtpEncoder::Ptr _encoder;
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
    void createRtpEncoder(uint32_t ssrc, int mtu) override {}
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
            int bitrate = 4000) {

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
    }

    CodecId getCodecId() const override {
        return CodecH264;
    }

    void createRtpEncoder(uint32_t ssrc, int mtu) override{
        _encoder = std::make_shared<H264RtpEncoder>(ssrc,
                                                    mtu,
                                                    _sample_rate,
                                                    _playload_type,
                                                    _track_id * 2);
    }
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
    CodecId getCodecId() const override {
        return CodecAAC;
    }

    void createRtpEncoder(uint32_t ssrc,
                          int mtu) override{
        _encoder = std::make_shared<AACRtpEncoder>(ssrc,
                                                   mtu,
                                                   _sample_rate,
                                                   _playload_type,
                                                   _track_id * 2);
    }

    void setFrameRing(const FrameRingInterface::RingType::Ptr &ring) override{
        if(_encoder){
            _encoder->setFrameRing(ring);
        }
    }
    void setRtpRing(const RtpRingInterface::RingType::Ptr &ring) override{
        if(_encoder){
            _encoder->setRtpRing(ring);
        }
    }
private:
    _StrPrinter _printer;
    int _playload_type;
    int _sample_rate;
    int _track_id;
};

/**
* rtsp生成器
*/
class RtspEncoder : public FrameRingInterface , public RtpRingInterface{
public:
    /**
     * 构成函数
     */
    RtspEncoder(){
        //自适应缓存
        _rtpRing = std::make_shared<RtpRingInterface::RingType>(0);
        //禁用缓存
        _frameRing = std::make_shared<FrameRingInterface::RingType>(1);
    }
    virtual ~RtspEncoder(){}

    /**
     * 添加音视频track
     * @param sdp 媒体描述对象
     * @param ssrc 媒体rtp ssrc
     * @param mtu 媒体rtp mtu
     */
    void addTrack(const Sdp::Ptr & sdp,uint32_t ssrc = 0,int mtu = 1400){
        if(ssrc == 0){
            ssrc = ((uint64_t) sdp.get()) & 0xFFFFFFFF;
        }
        sdp->createRtpEncoder(ssrc, mtu);
        sdp->setFrameRing(_frameRing);
        sdp->setRtpRing(_rtpRing);
        _sdp_map[sdp->getTrackType()] = sdp;
    }


    /**
     * 获取完整的SDP字符串
     * @return SDP字符串
     */
    string getSdp() {
        _StrPrinter printer;
        for(auto &pr : _sdp_map){
            printer << pr.second->getSdp() ;
        }
        return printer;
    }


    /**
     * 写入帧数据然后打包rtp
     * @param frame 帧数据
     * @param key_pos 是否为关键帧
     */
    void inputFrame(const Frame::Ptr &frame,bool key_pos = true) override {
        auto it = _sdp_map.find(frame->getTrackType());
        if(it == _sdp_map.end()){
            return ;
        }
        it->second->inputFrame(frame,key_pos);
    }

     /**
      * 也可以在外部打包好rtp然后再写入
      * @param rtp rtp包
      * @param key_pos 是否为关键帧的第一个rtp包
      */
    void inputRtp(const RtpPacket::Ptr &rtp, bool key_pos = true) override {
        _rtpRing->write(rtp,key_pos);
    }

    /**
     * 获取rtp环形缓存
     * @return
     */
    RtpRingInterface::RingType::Ptr getRtpRing() const override{
        return  _rtpRing;
    }

    /**
     * 获取帧环形缓存
     * @return
     */
    FrameRingInterface::RingType::Ptr getFrameRing() const override{
        return _frameRing;
    }

    /**
     * 设置帧环形缓存
     * @param ring
     */
    void setFrameRing(const FrameRingInterface::RingType::Ptr &ring) override{
        _frameRing = ring;
        for(auto &pr : _sdp_map){
            pr.second->setFrameRing(ring);
        }
    }


    /**
     * 设置rtp环形缓存
     * @param ring
     */
    void setRtpRing(const RtpRingInterface::RingType::Ptr &ring) override{
        _rtpRing = ring;
        for(auto &pr : _sdp_map){
            pr.second->setRtpRing(ring);
        }
    }
private:
    map<int,Sdp::Ptr> _sdp_map;
    RtpRingInterface::RingType::Ptr _rtpRing;
    FrameRingInterface::RingType::Ptr _frameRing;
};


}
}



#endif //ZLMEDIAKIT_RTSPMAKER_H
