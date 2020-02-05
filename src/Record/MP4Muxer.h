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

#ifdef ENABLE_MP4RECORD

#include "Common/MediaSink.h"
#include "mov-writer.h"
#include "mpeg4-hevc.h"
#include "mpeg4-avc.h"
#include "mpeg4-aac.h"
#include "mov-buffer.h"
#include "mov-format.h"
#include "Extension/AAC.h"
#include "Extension/H264.h"
#include "Extension/H265.h"
#include "Common/Stamp.h"

namespace mediakit{

class MP4MuxerBase{
public:
    MP4MuxerBase() = default;
    virtual ~MP4MuxerBase() = default;
protected:
    virtual int onRead(void* data, uint64_t bytes) = 0;
    virtual int onWrite(const void* data, uint64_t bytes) = 0;
    virtual int onSeek( uint64_t offset) = 0;
    virtual uint64_t onTell() = 0;
    void init(int flags);
protected:
    std::shared_ptr<mov_writer_t> _mov_writter;
};

class MP4Muxer : public MediaSinkInterface , public MP4MuxerBase{
public:
    MP4Muxer() = default;
    ~MP4Muxer() override = default;

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
    struct track_info{
        int track_id = -1;
        Stamp stamp;
    };
    unordered_map<int,track_info> _codec_to_trackid;
    List<Frame::Ptr> _frameCached;
    bool _started = false;
    bool _have_video = false;
};


class MP4MuxerFile : public MP4Muxer {
public:
    typedef std::shared_ptr<MP4MuxerFile> Ptr;
    MP4MuxerFile(const char *file);
    ~MP4MuxerFile();
    void resetTracks() override ;
protected:
    int onRead(void* data, uint64_t bytes) override;
    int onWrite(const void* data, uint64_t bytes) override;
    int onSeek( uint64_t offset) override;
    uint64_t onTell() override ;
    void openFile(const char *file);
private:
    std::shared_ptr<FILE> _file;
    string _file_name;
};

}//namespace mediakit

#endif//#ifdef ENABLE_MP4RECORD

#endif //ZLMEDIAKIT_MP4MUXER_H
