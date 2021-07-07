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
#include "TsMuxer.h"
namespace mediakit {

class HlsRecorder : public MediaSourceEventInterceptor, public TsMuxer, public std::enable_shared_from_this<HlsRecorder> {
public:
    typedef std::shared_ptr<HlsRecorder> Ptr;
    HlsRecorder(const string &m3u8_file, const string &params){
        GET_CONFIG(uint32_t, hlsNum, Hls::kSegmentNum);
        GET_CONFIG(uint32_t, hlsBufSize, Hls::kFileBufSize);
        GET_CONFIG(float, hlsDuration, Hls::kSegmentDuration);
        _hls = std::make_shared<HlsMakerImp>(m3u8_file, params, hlsBufSize, hlsDuration, hlsNum);
        //清空上次的残余文件
        _hls->clearCache();
    }

    ~HlsRecorder(){}

    void setMediaSource(const string &vhost, const string &app, const string &stream_id) {
        _hls->setMediaSource(vhost, app, stream_id);
    }

    void setListener(const std::weak_ptr<MediaSourceEvent> &listener) {
        setDelegate(listener);
        _hls->getMediaSource()->setListener(shared_from_this());
        //先注册媒体流，后续可以按需生成
        _hls->getMediaSource()->registHls(false);
    }

    int readerCount() {
        return _hls->getMediaSource()->readerCount();
    }

    void onReaderChanged(MediaSource &sender, int size) override {
        GET_CONFIG(bool, hls_demand, General::kHlsDemand);
        //hls保留切片个数为0时代表为hls录制(不删除切片)，那么不管有无观看者都一直生成hls
        _enabled = hls_demand ? (_hls->isLive() ? size : true) : true;
        if (!size && _hls->isLive() && hls_demand) {
            //hls直播时，如果无人观看就删除视频缓存，目的是为了防止视频跳跃
            _clear_cache = true;
        }
        MediaSourceEventInterceptor::onReaderChanged(sender, size);
    }

    bool isEnabled() {
        GET_CONFIG(bool, hls_demand, General::kHlsDemand);
        //缓存尚未清空时，还允许触发inputFrame函数，以便及时清空缓存
        return hls_demand ? (_clear_cache ? true : _enabled) : true;
    }

    void inputFrame(const Frame::Ptr &frame) override {
        GET_CONFIG(bool, hls_demand, General::kHlsDemand);
        if (_clear_cache && hls_demand) {
            _clear_cache = false;
            _hls->clearCache();
        }
        if (_enabled || !hls_demand) {
            TsMuxer::inputFrame(frame);
        }
    }

private:
    void onTs(std::shared_ptr<Buffer> buffer, uint32_t timestamp, bool is_idr_fast_packet) override {
        if (!buffer) {
            _hls->inputData(nullptr, 0, timestamp, is_idr_fast_packet);
        } else {
            _hls->inputData(buffer->data(), buffer->size(), timestamp, is_idr_fast_packet);
        }
    }

private:
    //默认不生成hls文件，有播放器时再生成
    bool _enabled = false;
    bool _clear_cache = false;
    std::shared_ptr<HlsMakerImp> _hls;
};
}//namespace mediakit
#endif //HLSRECORDER_H
