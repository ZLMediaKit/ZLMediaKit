#ifndef ZLMEDIAKIT_SRT_TRANSPORT_IMP_H
#define ZLMEDIAKIT_SRT_TRANSPORT_IMP_H

#include "Common/MultiMediaSourceMuxer.h"
#include "Rtp/Decoder.h"
#include "SrtTransport.hpp"
#include "TS/TSMediaSource.h"
#include <mutex>

namespace SRT {

using namespace std;
using namespace toolkit;
using namespace mediakit;
class SrtTransportImp
    : public SrtTransport
    , public toolkit::SockInfo
    , public MediaSinkInterface
    , public mediakit::MediaSourceEvent {
public:
    SrtTransportImp(const EventPoller::Ptr &poller);
    ~SrtTransportImp();

    void inputSockData(uint8_t *buf, int len, struct sockaddr_storage *addr) override {
        SrtTransport::inputSockData(buf, len, addr);
        _total_bytes += len;
    }
    void onSendTSData(const Buffer::Ptr &buffer, bool flush) override { SrtTransport::onSendTSData(buffer, flush); }
    /// SockInfo override
    std::string get_local_ip() override;
    uint16_t get_local_port() override;
    std::string get_peer_ip() override;
    uint16_t get_peer_port() override;
    std::string getIdentifier() const override;

protected:
    ///////SrtTransport override///////
    int getLatencyMul() override;
    int getPktBufSize() override;
    void onSRTData(DataPacket::Ptr pkt) override;
    void onShutdown(const SockException &ex) override;
    void onHandShakeFinished(std::string &streamid, struct sockaddr_storage *addr) override;

    void sendPacket(Buffer::Ptr pkt, bool flush = true) override {
        _total_bytes += pkt->size();
        SrtTransport::sendPacket(pkt, flush);
    }

    bool isPusher() override { return _is_pusher; }

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
    // get poller
    toolkit::EventPoller::Ptr getOwnerPoller(MediaSource &sender) override;

    ///////MediaSinkInterface override///////
    void resetTracks() override {};
    void addTrackCompleted() override;
    bool addTrack(const Track::Ptr &track) override;
    bool inputFrame(const Frame::Ptr &frame) override;

private:
    bool parseStreamid(std::string &streamid);
    void emitOnPublish();
    void emitOnPlay();

    void doPlay();
    void doCachedFunc();

private:
    bool _is_pusher = true;
    MediaInfo _media_info;
    uint64_t _total_bytes = 0;
    Ticker _alive_ticker;
    std::unique_ptr<sockaddr_storage> _addr;
    // for player
    TSMediaSource::RingType::RingReader::Ptr _ts_reader;
    // for pusher
    MultiMediaSourceMuxer::Ptr _muxer;
    DecoderImp::Ptr _decoder;
    std::recursive_mutex _func_mtx;
    std::deque<std::function<void()>> _cached_func;

    std::unordered_map<int, Stamp> _type_to_stamp;
};

} // namespace SRT

#endif // ZLMEDIAKIT_SRT_TRANSPORT_IMP_H
