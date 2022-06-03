#ifndef ZLMEDIAKIT_SRT_TRANSPORT_IMP_H
#define ZLMEDIAKIT_SRT_TRANSPORT_IMP_H

#include "Common/MultiMediaSourceMuxer.h"
#include "Rtp/Decoder.h"
#include "SrtTransport.hpp"

namespace SRT {
    using namespace toolkit;
    using namespace mediakit;
    using namespace std;
class SrtTransportImp
    : public SrtTransport
    , public toolkit::SockInfo
    , public MediaSinkInterface
    , public mediakit::MediaSourceEvent {
public:
    SrtTransportImp(const EventPoller::Ptr &poller);
    ~SrtTransportImp();
    
    /// SockInfo override
    std::string get_local_ip() override;
    uint16_t get_local_port() override;
    std::string get_peer_ip() override;
    uint16_t get_peer_port() override;
    std::string getIdentifier() const override;

protected:
    ///////SrtTransport override///////
    void onHandShakeFinished(std::string& streamid,struct sockaddr_storage *addr) override;
    void onSRTData(DataPacket::Ptr pkt,struct sockaddr_storage *addr) override;
    void onShutdown(const SockException &ex) override;

    ///////MediaSourceEvent override///////
    // 关闭
    bool close(mediakit::MediaSource &sender, bool force) override;
    // 播放总人数
    int totalReaderCount(mediakit::MediaSource &sender) override;
    // 获取媒体源类型
    mediakit::MediaOriginType getOriginType(mediakit::MediaSource &sender) const override;
    // 获取媒体源url或者文件路径
    std::string getOriginUrl(mediakit::MediaSource &sender) const override;
    // 获取媒体源客户端相关信息
    std::shared_ptr<SockInfo> getOriginSock(mediakit::MediaSource &sender) const override;

    bool inputFrame(const Frame::Ptr &frame) override;
    bool addTrack(const Track::Ptr & track) override;
    void addTrackCompleted() override;
    void resetTracks() override {};

private:
    void emitOnPublish();
    void emitOnPlay();

    void doPlay();
    void doCachedFunc();

private:
    bool _is_pusher = true;
    MediaInfo _media_info;

    std::unique_ptr<sockaddr_storage> _addr;

    // for pusher 
    MultiMediaSourceMuxer::Ptr _muxer;
    DecoderImp::Ptr _decoder;
    std::recursive_mutex _func_mtx;
    std::deque<std::function<void()> > _cached_func;
};

} // namespace SRT

#endif // ZLMEDIAKIT_SRT_TRANSPORT_IMP_H
