//
// Created by xzl on 2018/10/24.
//

#ifndef ZLMEDIAKIT_RTSPMAKER_H
#define ZLMEDIAKIT_RTSPMAKER_H

#include "RtspSdp.h"

namespace mediakit{
/**
* rtsp生成器
*/
class RtspMaker : public FrameRingInterface , public RtpRingInterface{
public:
    /**
     * 构成函数
     */
    RtspMaker(){
        _rtpRing = std::make_shared<RtpRingInterface::RingType>();
        _frameRing = std::make_shared<FrameRingInterface::RingType>();
    }
    virtual ~RtspMaker(){}

    /**
     * 添加音视频或title 媒体
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
     * 添加音视频或title 媒体
     * @param track 媒体描述
     * @param ssrc 媒体rtp ssrc
     * @param mtu 媒体rtp mtu
     */
    void addTrack(const Track::Ptr & track,uint32_t ssrc = 0,int mtu = 1400) ;

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
     */
    void inputFrame(const Frame::Ptr &frame) override {
        auto it = _sdp_map.find(frame->getTrackType());
        if(it == _sdp_map.end()){
            return ;
        }
        it->second->inputFrame(frame);
    }

    /**
     * 也可以在外部打包好rtp然后再写入
     * @param rtp rtp包
     * @param key_pos 是否为关键帧的第一个rtp包
     */
    bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos = true) override {
        auto it = _sdp_map.find(rtp->getTrackType());
        if(it == _sdp_map.end()){
            return false;
        }
        return it->second->inputRtp(rtp,key_pos);
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


} /* namespace mediakit */

#endif //ZLMEDIAKIT_RTSPMAKER_H
