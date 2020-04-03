/*
 * MIT License
 *
 * Copyright (c) 2016-2020 xiongziliang <771730766@qq.com>
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

#ifndef ZLMEDIAKIT_MP4DEMUXER_H
#define ZLMEDIAKIT_MP4DEMUXER_H
#ifdef ENABLE_MP4
#include "MP4.h"
#include "Extension/Track.h"
#include "Util/ResourcePool.h"
namespace mediakit {

class MP4Demuxer : public MP4File, public TrackSource{
public:
    typedef std::shared_ptr<MP4Demuxer> Ptr;
    MP4Demuxer(const char *file);
    ~MP4Demuxer() override;
    int64_t seekTo(int64_t stamp_ms);
    Frame::Ptr readFrame(bool &keyFrame, bool &eof);
    vector<Track::Ptr> getTracks(bool trackReady) const override ;
    uint64_t getDurationMS() const;
private:
    int getAllTracks();
    void onVideoTrack(uint32_t track_id, uint8_t object, int width, int height, const void* extra, size_t bytes);
    void onAudioTrack(uint32_t track_id, uint8_t object, int channel_count, int bit_per_sample, int sample_rate, const void* extra, size_t bytes);
    Frame::Ptr makeFrame(uint32_t track_id, const Buffer::Ptr &buf,int64_t pts, int64_t dts);
private:
    MP4File::Reader _mov_reader;
    uint64_t _duration_ms = 0;
    map<int, Track::Ptr > _track_to_codec;
    ResourcePool<BufferRaw> _buffer_pool;
};


}//namespace mediakit
#endif//ENABLE_MP4
#endif //ZLMEDIAKIT_MP4DEMUXER_H
