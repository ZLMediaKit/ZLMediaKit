/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MPEG_H
#define ZLMEDIAKIT_MPEG_H

#if defined(ENABLE_HLS) || defined(ENABLE_RTPPROXY)

#include <cstdio>
#include <cstdint>
#include <unordered_map>
#include "Extension/Frame.h"
#include "Extension/Track.h"
#include "Util/File.h"
#include "Common/MediaSink.h"
#include "Common/Stamp.h"

namespace mediakit {

//该类用于产生MPEG-TS/MPEG-PS
class MpegMuxer : public MediaSinkInterface {
public:
    MpegMuxer(bool is_ps);
    ~MpegMuxer() override;

    /**
     * 添加音视频轨道
     */
    bool addTrack(const Track::Ptr &track) override;

    /**
     * 重置音视频轨道
     */
    void resetTracks() override;

    /**
     * 输入帧数据
     */
    bool inputFrame(const Frame::Ptr &frame) override;

protected:
    /**
     * 输出ts/ps数据回调
     * @param buffer ts/ps数据包
     * @param timestamp 时间戳，单位毫秒
     * @param key_pos 是否为关键帧的第一个ts/ps包，用于确保ts切片第一帧为关键帧
     */
    virtual void onWrite(std::shared_ptr<toolkit::Buffer> buffer, uint64_t timestamp, bool key_pos) = 0;

private:
    void createContext();
    void releaseContext();
    void onWrite_l(const void *packet, size_t bytes);
    void flushCache();

private:
    bool _is_ps = false;
    bool _have_video = false;
    bool _key_pos = false;
    uint32_t _max_cache_size = 0;
    uint64_t _timestamp = 0;
    struct mpeg_muxer_t *_context = nullptr;
    std::unordered_map<int, int/*track_id*/> _codec_to_trackid;
    FrameMerger _frame_merger{FrameMerger::h264_prefix};
    toolkit::BufferRaw::Ptr _current_buffer;
    toolkit::ResourcePool<toolkit::BufferRaw> _buffer_pool;
};

}//mediakit

#else

#include "Common/MediaSink.h"

namespace mediakit {

class MpegMuxer : public MediaSinkInterface {
public:
    MpegMuxer(bool is_ps) {};
    ~MpegMuxer() override = default;
    bool addTrack(const Track::Ptr &track) override { return false; }
    void resetTracks() override {}
    bool inputFrame(const Frame::Ptr &frame) override { return false; }

protected:
    virtual void onWrite(std::shared_ptr<toolkit::Buffer> buffer, uint64_t timestamp, bool key_pos) = 0;
};

}//namespace mediakit

#endif

#endif //ZLMEDIAKIT_MPEG_H
