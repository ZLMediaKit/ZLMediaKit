/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
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
#include "Common/MediaSink.h"
#include "Util/ResourcePool.h"
namespace mediakit {

// 该类用于产生MPEG-TS/MPEG-PS  [AUTO-TRANSLATED:267efc85]
// This class is used to generate MPEG-TS/MPEG-PS
class MpegMuxer : public MediaSinkInterface {
public:
    MpegMuxer(bool is_ps = false);
    ~MpegMuxer() override;

    /**
     * 添加音视频轨道
     * Add audio and video tracks
     
     * [AUTO-TRANSLATED:7b0c1d64]
     */
    bool addTrack(const Track::Ptr &track) override;

    /**
     * 重置音视频轨道
     * Reset audio and video tracks
     
     * [AUTO-TRANSLATED:6eb1b742]
     */
    void resetTracks() override;

    /**
     * 输入帧数据
     * Input frame data
     
     * [AUTO-TRANSLATED:d13bc7f2]
     */
    bool inputFrame(const Frame::Ptr &frame) override;

    /**
     * 刷新输出所有frame缓存
     * Flush all frame buffers in the output
     
     * [AUTO-TRANSLATED:adaea568]
     */
    void flush() override;

protected:
    /**
     * 输出ts/ps数据回调
     * @param buffer ts/ps数据包
     * @param timestamp 时间戳，单位毫秒
     * @param key_pos 是否为关键帧的第一个ts/ps包，用于确保ts切片第一帧为关键帧
     * Callback for outputting ts/ps data
     * @param buffer ts/ps data packet
     * @param timestamp Timestamp, in milliseconds
     * @param key_pos Whether it is the first ts/ps packet of a key frame, used to ensure that the first frame of the ts slice is a key frame
     
     
     * [AUTO-TRANSLATED:dda8ed40]
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

    class FrameMergerImp : public FrameMerger {
    public:
        FrameMergerImp() : FrameMerger(FrameMerger::h264_prefix) {}
    };

    struct MP4Track {
        int track_id = -1;
        FrameMergerImp merger;
    };
    std::unordered_map<int, MP4Track> _tracks;
    toolkit::BufferRaw::Ptr _current_buffer;
    toolkit::ResourcePool<toolkit::BufferRaw> _buffer_pool;
};

}//mediakit

#else

#include "Common/MediaSink.h"

namespace mediakit {

class MpegMuxer : public MediaSinkInterface {
public:
    MpegMuxer(bool is_ps = false) {}
    bool addTrack(const Track::Ptr &track) override { return false; }
    void resetTracks() override {}
    bool inputFrame(const Frame::Ptr &frame) override { return false; }

protected:
    virtual void onWrite(std::shared_ptr<toolkit::Buffer> buffer, uint64_t timestamp, bool key_pos) = 0;
};

}//namespace mediakit

#endif

#endif //ZLMEDIAKIT_MPEG_H
