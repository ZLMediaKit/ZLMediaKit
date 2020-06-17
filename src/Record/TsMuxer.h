/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TSMUXER_H
#define TSMUXER_H

#include <unordered_map>
#include "Extension/Frame.h"
#include "Extension/Track.h"
#include "Util/File.h"
#include "Common/MediaSink.h"
#include "Common/Stamp.h"
using namespace toolkit;
namespace mediakit {

//该类用于产生MPEG-TS
class TsMuxer : public MediaSinkInterface {
public:
    TsMuxer();
    virtual ~TsMuxer();

    /**
     * 添加音视频轨道
     */
    void addTrack(const Track::Ptr &track) override;

    /**
     * 重置音视频轨道
     */
    void resetTracks() override;

    /**
     * 输入帧数据
     */
    void inputFrame(const Frame::Ptr &frame) override;

protected:
    /**
     * 输出mpegts数据回调
     * @param packet mpegts数据
     * @param bytes mpegts数据长度
     * @param timestamp 时间戳，单位毫秒
     * @param is_idr_fast_packet 是否为关键帧的第一个TS包，用于确保ts切片第一帧为关键帧
     */
    virtual void onTs(const void *packet, int bytes,uint32_t timestamp,bool is_idr_fast_packet) = 0;

private:
    void init();
    void uninit();
    //音视频时间戳同步用
    void stampSync();

private:
    void *_context = nullptr;
    char _tsbuf[188];
    uint32_t _timestamp = 0;
    struct track_info {
        int track_id = -1;
        Stamp stamp;
    };
    unordered_map<int, track_info> _codec_to_trackid;
    List<Frame::Ptr> _frameCached;
    bool _is_idr_fast_packet = false;
    bool _have_video = false;
};

}//namespace mediakit
#endif //TSMUXER_H