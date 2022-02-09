/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MP4DEMUXER_H
#define ZLMEDIAKIT_MP4DEMUXER_H
#ifdef ENABLE_MP4
#include "MP4.h"
#include "Extension/Track.h"
#include "Util/ResourcePool.h"
namespace mediakit {

class MP4Demuxer : public TrackSource {
public:
    typedef std::shared_ptr<MP4Demuxer> Ptr;

    /**
     * 创建mp4解复用器
     */
    MP4Demuxer();
    ~MP4Demuxer() override;

    /**
     * 打开文件
     * @param file mp4文件路径
     */
    void openMP4(const std::string &file);

    /**
     * @brief 关闭 mp4 文件
     */
    void closeMP4();

    /**
     * 移动时间轴至某处
     * @param stamp_ms 预期的时间轴位置，单位毫秒
     * @return 时间轴位置
     */
    int64_t seekTo(int64_t stamp_ms);

    /**
     * 读取一帧数据
     * @param keyFrame 是否为关键帧
     * @param eof 是否文件读取完毕
     * @return 帧数据,可能为空
     */
    Frame::Ptr readFrame(bool &keyFrame, bool &eof);

    /**
     * 获取所有Track信息
     * @param trackReady 是否要求track为就绪状态
     * @return 所有Track
     */
    std::vector<Track::Ptr> getTracks(bool trackReady) const override;

    /**
     * 获取文件长度
     * @return 文件长度，单位毫秒
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
    std::map<int, Track::Ptr> _track_to_codec;
    toolkit::ResourcePool<toolkit::BufferRaw> _buffer_pool;
};


}//namespace mediakit
#endif//ENABLE_MP4
#endif //ZLMEDIAKIT_MP4DEMUXER_H
