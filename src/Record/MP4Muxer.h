/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef ZLMEDIAKIT_MP4MUXER_H
#define ZLMEDIAKIT_MP4MUXER_H

#ifdef ENABLE_MP4

#include "Common/MediaSink.h"
#include "Extension/AAC.h"
#include "Extension/H264.h"
#include "Extension/H265.h"
#include "Common/Stamp.h"
#include "MP4.h"

namespace mediakit{

class MP4Muxer : public MediaSinkInterface, public MP4File{
public:
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

private:
    void openMP4();
    void closeMP4();

private:
    struct track_info{
        int track_id = -1;
        Stamp stamp;
    };
    unordered_map<int,track_info> _codec_to_trackid;
    List<Frame::Ptr> _frameCached;
    bool _started = false;
    bool _have_video = false;
    MP4File::Writer _mov_writter;
    string _file_name;
};

}//namespace mediakit
#endif//#ifdef ENABLE_MP4
#endif //ZLMEDIAKIT_MP4MUXER_H
