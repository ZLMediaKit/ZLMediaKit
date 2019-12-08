/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
    virtual void onWrite(const Buffer::Ptr &data) = 0;
    virtual void onDetach() = 0;
    virtual std::shared_ptr<FlvMuxer> getSharedPtr() = 0;
private:
    void onWriteFlvHeader(const RtmpMediaSource::Ptr &media);
    void onWriteRtmp(const RtmpPacket::Ptr &pkt);
    void onWriteFlvTag(const RtmpPacket::Ptr &pkt, uint32_t ui32TimeStamp);
    void onWriteFlvTag(uint8_t ui8Type, const Buffer::Ptr &buffer, uint32_t ui32TimeStamp);
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
    virtual void onWrite(const Buffer::Ptr &data) override ;
    virtual void onDetach() override;
    virtual std::shared_ptr<FlvMuxer> getSharedPtr() override;
private:
    std::shared_ptr<FILE> _file;
    recursive_mutex _file_mtx;
};


}//namespace mediakit

#endif //ZLMEDIAKIT_FLVMUXER_H
