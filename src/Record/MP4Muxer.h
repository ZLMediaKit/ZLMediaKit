/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MP4MUXER_H
#define ZLMEDIAKIT_MP4MUXER_H

#ifdef ENABLE_MP4

#include "Common/MediaSink.h"
#include "Extension/AAC.h"
#include "Extension/G711.h"
#include "Extension/H264.h"
#include "Extension/H265.h"
#include "Common/Stamp.h"
#include "MP4.h"

namespace mediakit{

class MP4Muxer : public MediaSinkInterface, public MP4File{
public:
    typedef std::shared_ptr<MP4Muxer> Ptr;

    MP4Muxer(const char *file);
    ~MP4Muxer() override;

    /**
     * 添加已经ready状态的track
     */
    void addTrack(const Track::Ptr & track) override;
    /**
     * 输入帧
     */
    void inputFrame(const Frame::Ptr &frame) override;

    /**
     * 重置所有track
     */
    void resetTracks() override ;

    /**
     * 手动关闭文件(对象析构时会自动关闭)
     */
    void closeMP4();

private:
    void openMP4();
    void stampSync();

private:
    struct track_info {
        int track_id = -1;
        Stamp stamp;
    };
    unordered_map<int, track_info> _codec_to_trackid;
    List<Frame::Ptr> _frameCached;
    bool _started = false;
    bool _have_video = false;
    MP4File::Writer _mov_writter;
    string _file_name;
};

}//namespace mediakit
#endif//#ifdef ENABLE_MP4
#endif //ZLMEDIAKIT_MP4MUXER_H
