/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_FLVMUXER_H
#define ZLMEDIAKIT_FLVMUXER_H

#include "Rtmp/Rtmp.h"
#include "Rtmp/RtmpMediaSource.h"
#include "Network/Socket.h"
#include "Common/Stamp.h"
using namespace toolkit;

namespace mediakit {

class FlvMuxer{
public:
    typedef std::shared_ptr<FlvMuxer> Ptr;
    FlvMuxer();
    virtual ~FlvMuxer();
    void stop();
protected:
    void start(const EventPoller::Ptr &poller,const RtmpMediaSource::Ptr &media);
    virtual void onWrite(const Buffer::Ptr &data, bool flush) = 0;
    virtual void onDetach() = 0;
    virtual std::shared_ptr<FlvMuxer> getSharedPtr() = 0;
private:
    void onWriteFlvHeader(const RtmpMediaSource::Ptr &media);
    void onWriteRtmp(const RtmpPacket::Ptr &pkt,bool flush);
    void onWriteFlvTag(const RtmpPacket::Ptr &pkt, uint32_t ui32TimeStamp, bool flush);
    void onWriteFlvTag(uint8_t ui8Type, const Buffer::Ptr &buffer, uint32_t ui32TimeStamp, bool flush);
private:
    RtmpMediaSource::RingType::RingReader::Ptr _ring_reader;
    //时间戳修整器
    Stamp _stamp[2];

};

class FlvRecorder : public FlvMuxer , public std::enable_shared_from_this<FlvRecorder>{
public:
    typedef std::shared_ptr<FlvRecorder> Ptr;
    FlvRecorder();
    virtual ~FlvRecorder();
    void startRecord(const EventPoller::Ptr &poller,const string &vhost,const string &app,const string &stream,const string &file_path);
    void startRecord(const EventPoller::Ptr &poller,const RtmpMediaSource::Ptr &media,const string &file_path);
private:
    virtual void onWrite(const Buffer::Ptr &data, bool flush) override ;
    virtual void onDetach() override;
    virtual std::shared_ptr<FlvMuxer> getSharedPtr() override;
private:
    std::shared_ptr<FILE> _file;
    recursive_mutex _file_mtx;
};


}//namespace mediakit

#endif //ZLMEDIAKIT_FLVMUXER_H
