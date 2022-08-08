/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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

class MP4MuxerInterface : public MediaSinkInterface {
public:
    MP4MuxerInterface() = default;
    ~MP4MuxerInterface() override = default;

    /**
     * 添加已经ready状态的track
     */
    bool addTrack(const Track::Ptr &track) override;

    /**
     * 输入帧
     */
    bool inputFrame(const Frame::Ptr &frame) override;

    /**
     * 重置所有track
     */
    void resetTracks() override;

    /**
     * 是否包含视频
     */
    bool haveVideo() const;

    /**
     * 保存fmp4分片
     */
    void saveSegment();

    /**
     * 创建新切片
     */
    void initSegment();

protected:
    virtual MP4FileIO::Writer createWriter() = 0;

private:
    void stampSync();

private:
    bool _started = false;
    bool _have_video = false;
    MP4FileIO::Writer _mov_writter;
    struct track_info {
        int track_id = -1;
        Stamp stamp;
    };
    std::unordered_map<int, track_info> _codec_to_trackid;
    FrameMerger _frame_merger{FrameMerger::mp4_nal_size};
};

class MP4Muxer : public MP4MuxerInterface{
public:
    typedef std::shared_ptr<MP4Muxer> Ptr;

    MP4Muxer();
    ~MP4Muxer() override;

    /**
     * 重置所有track
     */
    void resetTracks() override;

    /**
     * 打开mp4
     * @param file 文件完整路径
     */
    void openMP4(const std::string &file);

    /**
     * 手动关闭文件(对象析构时会自动关闭)
     */
    void closeMP4();

protected:
    MP4FileIO::Writer createWriter() override;

private:
    std::string _file_name;
    MP4FileDisk::Ptr _mp4_file;
};

class MP4MuxerMemory : public MP4MuxerInterface{
public:
    MP4MuxerMemory();
    ~MP4MuxerMemory() override = default;

    /**
     * 重置所有track
     */
    void resetTracks() override;

    /**
     * 输入帧
     */
    bool inputFrame(const Frame::Ptr &frame) override;

    /**
     * 获取fmp4 init segment
     */
    const std::string &getInitSegment();

protected:
    /**
     * 输出fmp4切片回调函数
     * @param std::string 切片内容
     * @param stamp 切片末尾时间戳
     * @param key_frame 是否有关键帧
     */
    virtual void onSegmentData(std::string string, uint64_t stamp, bool key_frame) = 0;

protected:
    MP4FileIO::Writer createWriter() override;

private:
    bool _key_frame = false;
    std::string _init_segment;
    MP4FileMemory::Ptr _memory_file;
};


}//namespace mediakit
#endif//#ifdef ENABLE_MP4
#endif //ZLMEDIAKIT_MP4MUXER_H
