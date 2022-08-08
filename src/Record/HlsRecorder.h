/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef HLSRECORDER_H
#define HLSRECORDER_H

#include "HlsMakerImp.h"
#include "MPEG.h"

namespace mediakit {

class HlsRecorder : public MediaSourceEventInterceptor, public MpegMuxer, public std::enable_shared_from_this<HlsRecorder> {
public:
    using Ptr = std::shared_ptr<HlsRecorder>;

    HlsRecorder(const std::string &m3u8_file, const std::string &params) : MpegMuxer(false) {
        GET_CONFIG(uint32_t, hlsNum, Hls::kSegmentNum);
        GET_CONFIG(bool, hlsKeep, Hls::kSegmentKeep);
        GET_CONFIG(uint32_t, hlsBufSize, Hls::kFileBufSize);
        GET_CONFIG(float, hlsDuration, Hls::kSegmentDuration);
        _hls = std::make_shared<HlsMakerImp>(m3u8_file, params, hlsBufSize, hlsDuration, hlsNum, hlsKeep);
        //清空上次的残余文件
        _hls->clearCache();
    }

    ~HlsRecorder() = default;

    void setMediaSource(const std::string &vhost, const std::string &app, const std::string &stream_id) {
        _hls->setMediaSource(vhost, app, stream_id);
    }

    void setListener(const std::weak_ptr<MediaSourceEvent> &listener) {
        setDelegate(listener);
        _hls->getMediaSource()->setListener(shared_from_this());
    }

    int readerCount() { return _hls->getMediaSource()->readerCount(); }

    void onReaderChanged(MediaSource &sender, int size) override {
        GET_CONFIG(bool, hls_demand, General::kHlsDemand);
        // hls保留切片个数为0时代表为hls录制(不删除切片)，那么不管有无观看者都一直生成hls
        _enabled = hls_demand ? (_hls->isLive() ? size : true) : true;
        if (!size && _hls->isLive() && hls_demand) {
            // hls直播时，如果无人观看就删除视频缓存，目的是为了防止视频跳跃
            _clear_cache = true;
        }
        MediaSourceEventInterceptor::onReaderChanged(sender, size);
    }

    bool inputFrame(const Frame::Ptr &frame) override {
        GET_CONFIG(bool, hls_demand, General::kHlsDemand);
        if (_clear_cache && hls_demand) {
            _clear_cache = false;
            //清空旧的m3u8索引文件于ts切片
            _hls->clearCache();
            _hls->getMediaSource()->setIndexFile("");
        }
        if (_enabled || !hls_demand) {
            return MpegMuxer::inputFrame(frame);
        }
        return false;
    }

    bool isEnabled() {
        GET_CONFIG(bool, hls_demand, General::kHlsDemand);
        //缓存尚未清空时，还允许触发inputFrame函数，以便及时清空缓存
        return hls_demand ? (_clear_cache ? true : _enabled) : true;
    }

private:
    void onWrite(std::shared_ptr<toolkit::Buffer> buffer, uint64_t timestamp, bool key_pos) override {
        if (!buffer) {
            _hls->inputData(nullptr, 0, timestamp, key_pos);
        } else {
            _hls->inputData(buffer->data(), buffer->size(), timestamp, key_pos);
        }
    }

private:
    bool _enabled = true;
    bool _clear_cache = false;
    std::shared_ptr<HlsMakerImp> _hls;
};
}//namespace mediakit
#endif //HLSRECORDER_H
