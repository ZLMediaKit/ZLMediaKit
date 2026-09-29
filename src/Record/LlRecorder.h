/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_LLRECORDER_H
#define ZLMEDIAKIT_LLRECORDER_H

#include "LlHlsMaker.h"
#include "DashCodec.h"
#include "MP4Muxer.h"

namespace mediakit {

/**
 * LL-HLS / LL-DASH 全内存录制器
 *
 * 与 HlsFMP4Recorder 的区别:
 *  - 不产生任何磁盘文件(无 init.mp4 / m4s / m3u8 / mpd 落盘)
 *  - 分片与播放列表全部保存在 LlSegmentStore 中, 由 HTTP 直接从内存读取
 *  - LL-HLS 与 LL-DASH 共用同一套内存分片
 *
 * Fully in-memory recorder for LL-HLS / LL-DASH
 *
 * Differences from HlsFMP4Recorder:
 *  - It never touches the disk (no init.mp4 / m4s / m3u8 / mpd files)
 *  - Segments and playlists live in the LlSegmentStore and are served from memory by HTTP
 *  - LL-HLS and LL-DASH share the same in-memory segments
 */
class LlFMP4Recorder final : public MediaSourceEventInterceptor,
                             public MP4MuxerMemory,
                             public std::enable_shared_from_this<LlFMP4Recorder> {
public:
    using Ptr = std::shared_ptr<LlFMP4Recorder>;

    LlFMP4Recorder(const MediaTuple &tuple, const ProtocolOption &) {
        auto config = LlConfig::load();
        _store = std::make_shared<LlSegmentStore>(config);
        _maker = std::make_shared<LlHlsMaker>(config, _store);
        _media_src = std::make_shared<LlMediaSource>(tuple, _store);
    }

    ~LlFMP4Recorder() override {
        try {
            MP4MuxerMemory::flush();
        } catch (std::exception &ex) {
            WarnL << ex.what();
        }
    }

    void setListener(const std::weak_ptr<MediaSourceEvent> &listener) {
        setDelegate(listener);
        _media_src->setListener(shared_from_this());
    }

    int readerCount() const {
        return _media_src->readerCount();
    }

    void onReaderChanged(MediaSource &sender, int size) override {
        MediaSourceEventInterceptor::onReaderChanged(sender, size);
    }

    /**
     * LL持续生产, 不做按需: LL播放器按part粒度轮询播放列表,
     * 按需启停会导致分片反复归零、首屏必然等待一个完整分片
     * LL produces continuously and is never generated on demand: LL players poll the
     * playlist at part granularity, toggling production would reset the segments
     * repeatedly and force the first frame to wait for a full segment
     */
    bool isEnabled() { return true; }

    MediaSource::Ptr getMediaSource() const {
        return _media_src;
    }

    void addTrackCompleted() override {
        MP4MuxerMemory::addTrackCompleted();
        auto init = getInitSegment();
        // 首次产出init segment即注册媒体源, 使HTTP可以按url找到本流
        // Register the media source as soon as the init segment exists, so HTTP can find it
        if (!init.empty()) {
            _store->setInitSegment(init);
            _store->setHaveVideo(haveVideo());
            _store->setCodecs(_codecs);
            _media_src->registRing();
        }
    }

    bool addTrack(const Track::Ptr &track) override {
        // 累加dash codecs属性, 在addTrackCompleted()中定稿
        // Accumulate the dash codecs attribute, finalized in addTrackCompleted()
        if (track->ready()) {
            auto cs = getDashCodecString(track);
            if (!cs.empty()) {
                if (!_codecs.empty()) {
                    _codecs += ",";
                }
                _codecs += cs;
            }
        }
        return MP4MuxerMemory::addTrack(track);
    }

    void resetTracks() override {
        MP4MuxerMemory::resetTracks();
        _codecs.clear();
        if (_maker) {
            _maker->clear();
        }
        if (_store) {
            _store->clear();
        }
    }

protected:
    void onSegmentData(std::string buffer, uint64_t, bool key_pos) override {
        if (!_maker) {
            return;
        }
        // MP4MuxerMemory回调参数stamp是输入帧的原始DTS; MP4写入前会经Stamp::revise修正时间戳。
        // 分片切分及DASH时间轴必须使用与fMP4中的tfdt一致的相对媒体时间，避免两条时钟逐片漂移。
        // The callback stamp is the input frame's raw DTS, while MP4 timestamps are rewritten by Stamp::revise.
        // Use the relative media clock that was written into tfdt, so segment boundaries and DASH timing
        // cannot drift from the timestamps in the generated fMP4.
        auto media_stamp = getDuration();
        if (buffer.empty()) {
            _maker->inputData(nullptr, media_stamp);
        } else {
            auto chunk = std::make_shared<LlCmafChunk>();
            chunk->data = std::make_shared<toolkit::BufferString>(std::move(buffer));
            chunk->stamp = media_stamp;
            chunk->key_pos = key_pos;
            _media_src->inputCmafChunk(chunk);
            _maker->inputData(chunk);
        }
    }

private:
    std::string _codecs;
    LlSegmentStore::Ptr _store;
    LlHlsMaker::Ptr _maker;
    LlMediaSource::Ptr _media_src;
};

} // namespace mediakit
#endif // ZLMEDIAKIT_LLRECORDER_H
