/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MP4MUXER_H
#define ZLMEDIAKIT_MP4MUXER_H

#if defined(ENABLE_MP4)

#include "Common/MediaSink.h"
#include "Common/Stamp.h"
#include "MP4.h"

namespace mediakit {

class MP4MuxerInterface : public MediaSinkInterface {
public:

    /**
     * 添加已经ready状态的track
     * Add tracks that are in ready state
     
     * [AUTO-TRANSLATED:ea4983df]
     */
    bool addTrack(const Track::Ptr &track) override;

    /**
     * 输入帧
     * Input frame
     
     * [AUTO-TRANSLATED:c91b5ec6]
     */
    bool inputFrame(const Frame::Ptr &frame) override;

    /**
     * 重置所有track
     * Reset all tracks
     
     * [AUTO-TRANSLATED:f203fa3e]
     */
    void resetTracks() override;

    /**
     * 刷新输出所有frame缓存
     * Refresh all frame cache output
     
     * [AUTO-TRANSLATED:adaea568]
     */
    void flush() override;

    /**
     * 是否包含视频
     * Whether it contains video
     
     * [AUTO-TRANSLATED:6d9e1039]
     */
    bool haveVideo() const;

    /**
     * 保存fmp4分片
     * Save fmp4 fragment
     
     * [AUTO-TRANSLATED:7b808759]
     */
    void saveSegment();

    /**
     * 创建新切片
     * Create new fragment
     
     * [AUTO-TRANSLATED:b27545cf]
     */
    void initSegment();

    /**
     * 获取mp4时长,单位毫秒
     * Get mp4 duration, in milliseconds
     
     * [AUTO-TRANSLATED:d87afcfb]
     */
    uint64_t getDuration() const;

protected:
    virtual MP4FileIO::Writer createWriter() = 0;

private:
    void stampSync();

private:
    bool _started = false;
    bool _have_video = false;
    MP4FileIO::Writer _mov_writter;
    int _non_iframe_video_count; // 非I帧个数

    class FrameMergerImp : public FrameMerger {
    public:
        FrameMergerImp() : FrameMerger(FrameMerger::mp4_nal_size) {}
    };

    struct MP4Track {
        int track_id = -1;
        Stamp stamp;
        FrameMergerImp merger;
    };
    std::unordered_map<int, MP4Track> _tracks;
};

class MP4Muxer : public MP4MuxerInterface{
public:
    using Ptr = std::shared_ptr<MP4Muxer>;
    ~MP4Muxer() override;
    /**
     * 重置所有track
     * Reset all tracks
     
     * [AUTO-TRANSLATED:f203fa3e]
     */
    void resetTracks() override;

    /**
     * 打开mp4
     * @param file 文件完整路径
     * Open mp4
     * @param file Full file path
     
     * [AUTO-TRANSLATED:416892f4]
     */
    void openMP4(const std::string &file);

    /**
     * 手动关闭文件(对象析构时会自动关闭)
     * Manually close the file (it will be closed automatically when the object is destructed)
     
     * [AUTO-TRANSLATED:9ca68ff9]
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

    /**
     * 重置所有track
     * Reset all tracks
     
     * [AUTO-TRANSLATED:f203fa3e]
     */
    void resetTracks() override;

    /**
     * 输入帧
     * Input frame
     
     * [AUTO-TRANSLATED:c91b5ec6]
     */
    bool inputFrame(const Frame::Ptr &frame) override;

    /**
     * 获取fmp4 init segment
     * Get fmp4 init segment
     
     * [AUTO-TRANSLATED:6c704ec9]
     */
    const std::string &getInitSegment();

protected:
    /**
     * 输出fmp4切片回调函数
     * @param std::string 切片内容
     * @param stamp 切片末尾时间戳
     * @param key_frame 是否有关键帧
     * Output fmp4 fragment callback function
     * @param std::string Fragment content
     * @param stamp Fragment end timestamp
     * @param key_frame Whether there is a key frame
     
     * [AUTO-TRANSLATED:dd742da5]
     */
    virtual void onSegmentData(std::string string, uint64_t stamp, bool key_frame) = 0;

protected:
    MP4FileIO::Writer createWriter() override;

private:
    bool _key_frame = false;
    uint64_t _last_dst = 0;
    std::string _init_segment;
    MP4FileMemory::Ptr _memory_file;
};

} // namespace mediakit

#else

#include "Common/MediaSink.h"

namespace mediakit {

class MP4MuxerMemory : public MediaSinkInterface {
public:
    bool addTrack(const Track::Ptr & track) override { return false; }
    bool inputFrame(const Frame::Ptr &frame) override { return false; }
    const std::string &getInitSegment() { static std::string kNull; return kNull; };

protected:
    /**
     * 输出fmp4切片回调函数
     * @param std::string 切片内容
     * @param stamp 切片末尾时间戳
     * @param key_frame 是否有关键帧
     * Output fmp4 fragment callback function
     * @param std::string Fragment content
     * @param stamp Fragment end timestamp
     * @param key_frame Whether there is a key frame
     
     
     * [AUTO-TRANSLATED:dd742da5]
     */
    virtual void onSegmentData(std::string string, uint64_t stamp, bool key_frame) = 0;
};

} // namespace mediakit

#endif //defined(ENABLE_MP4)
#endif //ZLMEDIAKIT_MP4MUXER_H
