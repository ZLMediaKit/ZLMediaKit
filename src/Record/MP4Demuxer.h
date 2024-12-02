/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MP4DEMUXER_H
#define ZLMEDIAKIT_MP4DEMUXER_H
#ifdef ENABLE_MP4

#include <map>
#include "MP4.h"
#include "Extension/Track.h"
#include "Util/ResourcePool.h"

namespace mediakit {

class MP4Demuxer : public TrackSource {
public:
    using Ptr = std::shared_ptr<MP4Demuxer>;

    ~MP4Demuxer() override;

    /**
     * 打开文件
     * @param file mp4文件路径
     * Open file
     * @param file mp4 file path
     
     * [AUTO-TRANSLATED:a64c5a6b]
     */
    void openMP4(const std::string &file);

    /**
     * @brief 关闭 mp4 文件
     * @brief Close mp4 file
     
     * [AUTO-TRANSLATED:527865d9]
     */
    void closeMP4();

    /**
     * 移动时间轴至某处
     * @param stamp_ms 预期的时间轴位置，单位毫秒
     * @return 时间轴位置
     * Move timeline to a specific location
     * @param stamp_ms Expected timeline position, in milliseconds
     * @return Timeline position
     
     * [AUTO-TRANSLATED:51ce0f6d]
     */
    int64_t seekTo(int64_t stamp_ms);

    /**
     * 读取一帧数据
     * @param keyFrame 是否为关键帧
     * @param eof 是否文件读取完毕
     * @return 帧数据,可能为空
     * Read a frame of data
     * @param keyFrame Whether it is a key frame
     * @param eof Whether the file has been read completely
     * @return Frame data, may be empty
     
     * [AUTO-TRANSLATED:adf550de]
     */
    Frame::Ptr readFrame(bool &keyFrame, bool &eof);

    /**
     * 获取所有Track信息
     * @param trackReady 是否要求track为就绪状态
     * @return 所有Track
     * Get all Track information
     * @param trackReady Whether to require the track to be ready
     * @return All Tracks
     
     * [AUTO-TRANSLATED:c07ad51a]
     */
    std::vector<Track::Ptr> getTracks(bool trackReady) const override;

    /**
     * 获取文件长度
     * @return 文件长度，单位毫秒
     * Get file length
     * @return File length, in milliseconds
     
     
     * [AUTO-TRANSLATED:dcd865d6]
     */
    uint64_t getDurationMS() const;

private:
    int getAllTracks();
    void onVideoTrack(uint32_t track_id, uint8_t object, int width, int height, const void *extra, size_t bytes);
    void onAudioTrack(uint32_t track_id, uint8_t object, int channel_count, int bit_per_sample, int sample_rate, const void *extra, size_t bytes);
    Frame::Ptr makeFrame(uint32_t track_id, const toolkit::Buffer::Ptr &buf, int64_t pts, int64_t dts);

private:
    MP4FileDisk::Ptr _mp4_file;
    MP4FileDisk::Reader _mov_reader;
    uint64_t _duration_ms = 0;
    std::unordered_map<int, Track::Ptr> _tracks;
    toolkit::ResourcePool<toolkit::BufferRaw> _buffer_pool;
};

class MultiMP4Demuxer : public TrackSource {
public:
    using Ptr = std::shared_ptr<MultiMP4Demuxer>;

    ~MultiMP4Demuxer() override = default;

    /**
     * 批量打开mp4文件，把多个文件当做一个mp4看待
     * @param file 多个mp4文件路径，以分号分隔; 或者包含多个mp4文件的文件夹
     */
    void openMP4(const std::string &file);

    /**
     * @brief 批量关闭 mp4 文件
     */
    void closeMP4();

    /**
     * 移动总体时间轴至某处
     * @param stamp_ms 预期的时间轴总体长度位置，单位毫秒
     * @return 时间轴总体长度位置
     */
    int64_t seekTo(int64_t stamp_ms);

    /**
     * 读取一帧数据
     * @param keyFrame 是否为关键帧
     * @param eof 是否所有文件读取完毕
     * @return 帧数据,可能为空
     */
    Frame::Ptr readFrame(bool &keyFrame, bool &eof);

    /**
     * 获取第一个文件所有Track信息
     * @param trackReady 是否要求track为就绪状态
     * @return 第一个文件所有Track信息
     */
    std::vector<Track::Ptr> getTracks(bool trackReady) const override;

    /**
     * 获取文件总长度
     * @return 文件总长度，单位毫秒
     */
    uint64_t getDurationMS() const;

private:
    std::map<int, Track::Ptr> _tracks;
    std::map<uint64_t, MP4Demuxer::Ptr>::iterator _it;
    std::map<uint64_t, MP4Demuxer::Ptr> _demuxers;
};

}//namespace mediakit
#endif//ENABLE_MP4
#endif //ZLMEDIAKIT_MP4DEMUXER_H
