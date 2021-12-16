/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef HLSRECORDER_DISK_H
#define HLSRECORDER_DISK_H

#include "Common/MediaSink.h"
#include "HlsMakerImp.h"
#include "TsMuxer.h"

namespace mediakit {

class HlsRecorderDisk : public TsMuxer, public std::enable_shared_from_this<HlsRecorderDisk> {
public:
    typedef std::shared_ptr<HlsRecorderDisk> Ptr;
    HlsRecorderDisk(const string &m3u8_file, const string &params){
        GET_CONFIG(uint32_t, hlsNum, Hls::kSegmentNum);
        GET_CONFIG(uint32_t, hlsBufSize, Hls::kFileBufSize);
        GET_CONFIG(float, hlsDuration, Hls::kSegmentDuration);
        _hls = std::make_shared<HlsMakerImp>(m3u8_file, params, hlsBufSize, hlsDuration,
                                             hlsNum, Recorder::type_hls_disk);
        //清空上次的残余文件
        _hls->clearCache(false, true);
    }

    ~HlsRecorderDisk() override= default;

    /**
     * 重置所有Track
     */
    void resetTracks() override;

    /**
     * 输入frame
     */
    //bool inputFrame(const Frame::Ptr &frame) override;

    /**
     * 添加ready状态的track
     */
    bool addTrack(const Track::Ptr & track) override;

    void setMediaSource(const string &vhost, const string &app, const string &stream_id) {
        _hls->setMediaSource(vhost, app, stream_id);
    }

    bool inputFrame(const Frame::Ptr &frame) override {;
        if (_clear_cache) {
            _clear_cache = false;
            _hls->clearCache();
        }

        return TsMuxer::inputFrame(frame);
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
    bool _clear_cache = false;
    std::shared_ptr<HlsMakerImp> _hls;
};
}//namespace mediakit
#endif //HLSRECORDER_DISK_H
