/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef HLSRECORDER_H
#define HLSRECORDER_H

#include "HlsMakerImp.h"
#include "MPEG.h"
#include "MP4Muxer.h"
#include "Common/config.h"

namespace mediakit {

template <typename Muxer>
class HlsRecorderBase : public MediaSourceEventInterceptor, public Muxer, public std::enable_shared_from_this<HlsRecorderBase<Muxer> > {
public:
    HlsRecorderBase(bool is_fmp4, const std::string &m3u8_file, const std::string &params, const ProtocolOption &option) {
        GET_CONFIG(uint32_t, hlsNum, Hls::kSegmentNum);
        GET_CONFIG(bool, hlsKeep, Hls::kSegmentKeep);
        GET_CONFIG(uint32_t, hlsBufSize, Hls::kFileBufSize);
        GET_CONFIG(float, hlsDuration, Hls::kSegmentDuration);

        _option = option;
        _hls = std::make_shared<HlsMakerImp>(is_fmp4, m3u8_file, params, hlsBufSize, hlsDuration, hlsNum, hlsKeep);
        // 清空上次的残余文件
        _hls->clearCache();
    }

    void setMediaSource(const MediaTuple& tuple) {
        _hls->setMediaSource(tuple);
    }

    void setListener(const std::weak_ptr<MediaSourceEvent> &listener) {
        setDelegate(listener);
        _hls->getMediaSource()->setListener(this->shared_from_this());
    }

    int readerCount() { return _hls->getMediaSource()->readerCount(); }

    void onReaderChanged(MediaSource &sender, int size) override {
        // hls保留切片个数为0时代表为hls录制(不删除切片)，那么不管有无观看者都一直生成hls
        _enabled = _option.hls_demand ? (_hls->isLive() ? size : true) : true;
        if (!size && _hls->isLive() && _option.hls_demand) {
            // hls直播时，如果无人观看就删除视频缓存，目的是为了防止视频跳跃
            _clear_cache = true;
        }
        MediaSourceEventInterceptor::onReaderChanged(sender, size);
    }

    bool inputFrame(const Frame::Ptr &frame) override {
        if (_clear_cache && _option.hls_demand) {
            _clear_cache = false;
            //清空旧的m3u8索引文件于ts切片
            _hls->clearCache();
            _hls->getMediaSource()->setIndexFile("");
        }
        if (_enabled || !_option.hls_demand) {
            return Muxer::inputFrame(frame);
        }
        return false;
    }

    bool isEnabled() {
        //缓存尚未清空时，还允许触发inputFrame函数，以便及时清空缓存
        return _option.hls_demand ? (_clear_cache ? true : _enabled) : true;
    }

protected:
    bool _enabled = true;
    bool _clear_cache = false;
    ProtocolOption _option;
    std::shared_ptr<HlsMakerImp> _hls;
};

class HlsRecorder final : public HlsRecorderBase<MpegMuxer> {
public:
    using Ptr = std::shared_ptr<HlsRecorder>;
    template <typename ...ARGS>
    HlsRecorder(ARGS && ...args) : HlsRecorderBase<MpegMuxer>(false, std::forward<ARGS>(args)...) {}
    ~HlsRecorder() override { this->flush(); }

private:
    void onWrite(std::shared_ptr<toolkit::Buffer> buffer, uint64_t timestamp, bool key_pos) override {
        if (!buffer) {
            // reset tracks
            _hls->inputData(nullptr, 0, timestamp, key_pos);
        } else {
            _hls->inputData(buffer->data(), buffer->size(), timestamp, key_pos);
        }
    }
};

class HlsFMP4Recorder final : public HlsRecorderBase<MP4MuxerMemory> {
public:
    using Ptr = std::shared_ptr<HlsFMP4Recorder>;
    template <typename ...ARGS>
    HlsFMP4Recorder(ARGS && ...args) : HlsRecorderBase<MP4MuxerMemory>(true, std::forward<ARGS>(args)...) {}
    ~HlsFMP4Recorder() override { this->flush(); }

    void addTrackCompleted() override {
        HlsRecorderBase<MP4MuxerMemory>::addTrackCompleted();
        auto data = getInitSegment();
        _hls->inputInitSegment(data.data(), data.size());
    }

private:
    void onSegmentData(std::string buffer, uint64_t timestamp, bool key_pos) override {
        if (buffer.empty()) {
            // reset tracks
            _hls->inputData(nullptr, 0, timestamp, key_pos);
        } else {
            _hls->inputData((char *)buffer.data(), buffer.size(), timestamp, key_pos);
        }
    }
};

}//namespace mediakit
#endif //HLSRECORDER_H
