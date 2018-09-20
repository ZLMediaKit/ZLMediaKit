//
// Created by xzl on 2018/8/30.
//

#ifndef ZLMEDIAKIT_FLVRECORDER_H
#define ZLMEDIAKIT_FLVRECORDER_H

#include "Rtmp.h"
#include "RtmpMediaSource.h"
#include "Network/Socket.h"

using namespace ZL::Network;

namespace ZL {
namespace Rtmp {

class FlvMuxer{
public:
    typedef std::shared_ptr<FlvMuxer> Ptr;
    FlvMuxer();
    virtual ~FlvMuxer();
    void stop();
protected:
    void start(const RtmpMediaSource::Ptr &media);
    virtual void onWrite(const Buffer::Ptr &data) = 0;
    virtual void onWrite(const char *data,int len) = 0;
    virtual void onDetach() = 0;
    virtual std::shared_ptr<FlvMuxer> getSharedPtr() = 0;
private:
    void onWriteFlvHeader(const RtmpMediaSource::Ptr &media);
    void onWriteRtmp(const RtmpPacket::Ptr &pkt);
    void onWriteFlvTag(const RtmpPacket::Ptr &pkt, uint32_t ui32TimeStamp);
    void onWriteFlvTag(uint8_t ui8Type, const std::string &strBuf, uint32_t ui32TimeStamp);
private:
    RtmpMediaSource::RingType::RingReader::Ptr _ring_reader;
    uint32_t m_aui32FirstStamp[2] = {0};
    uint32_t m_previousTagSize = 0;
};

class FlvRecorder : public FlvMuxer , public std::enable_shared_from_this<FlvRecorder>{
public:
    typedef std::shared_ptr<FlvRecorder> Ptr;
    FlvRecorder();
    virtual ~FlvRecorder();
    void startRecord(const string &vhost,const string &app,const string &stream,const string &file_path);
    void startRecord(const RtmpMediaSource::Ptr &media,const string &file_path);
private:
    virtual void onWrite(const Buffer::Ptr &data) override ;
    virtual void onWrite(const char *data,int len) override;
    virtual void onDetach() override;
    virtual std::shared_ptr<FlvMuxer> getSharedPtr() override;
private:
    std::shared_ptr<FILE> _file;
    recursive_mutex _file_mtx;
};


}//namespace Rtmp
}//namespace ZL

#endif //ZLMEDIAKIT_FLVRECORDER_H
