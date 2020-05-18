/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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
