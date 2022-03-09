/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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

namespace mediakit {
// 此类将RtmpMediaSource中的RtmpPacket，串行化到onWrite的二进制toolkit::Buffer
class FlvMuxer {
public:
    using Ptr = std::shared_ptr<FlvMuxer>;
    FlvMuxer();
    virtual ~FlvMuxer() = default;

    void stop();

protected:
    // 不同线程延迟2s，写头部，注册帧回调 RingReader.
    void start(const toolkit::EventPoller::Ptr &poller, const RtmpMediaSource::Ptr &media, uint32_t start_pts = 0);

    // 子类重载函数.
    // 写入二进制数据
    virtual void onWrite(const toolkit::Buffer::Ptr &data, bool flush) = 0;
    // stop或RtmpMediaSource Detach时回调
    virtual void onDetach() = 0;
    // 为了跨线程投递, 为啥不 enable_shared_from_this?
    virtual std::shared_ptr<FlvMuxer> getSharedPtr() = 0;

private:
    // 写入Flv 9字节头部，MetaData，config frame
    void onWriteFlvHeader(const RtmpMediaSource::Ptr &src);
    // 写Rtmp数据包
    void onWriteRtmp(const RtmpPacket::Ptr &pkt, bool flush);

    void onWriteFlvTag(uint8_t type, const toolkit::Buffer::Ptr &buffer, uint32_t time_stamp, bool flush);
    toolkit::BufferRaw::Ptr obtainBuffer(const void *data, size_t len);
    toolkit::BufferRaw::Ptr obtainBuffer();

private:
    toolkit::ResourcePool<toolkit::BufferRaw> _packet_pool;
    RtmpMediaSource::RingType::RingReader::Ptr _ring_reader;
};

class FlvRecorder : public FlvMuxer , public std::enable_shared_from_this<FlvRecorder>{
public:
    using Ptr = std::shared_ptr<FlvRecorder>;
    FlvRecorder() = default;
    ~FlvRecorder() override = default;

    void startRecord(const toolkit::EventPoller::Ptr &poller, const RtmpMediaSource::Ptr &media, const std::string &file_path);
    void startRecord(const toolkit::EventPoller::Ptr &poller, const std::string &vhost, const std::string &app, const std::string &stream, const std::string &file_path);

private:
    virtual void onWrite(const toolkit::Buffer::Ptr &data, bool flush) override ;
    virtual void onDetach() override;
    virtual std::shared_ptr<FlvMuxer> getSharedPtr() override;

private:
    std::shared_ptr<FILE> _file;
    std::recursive_mutex _file_mtx;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_FLVMUXER_H
