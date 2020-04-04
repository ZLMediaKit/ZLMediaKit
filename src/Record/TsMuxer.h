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

class TsMuxer : public MediaSinkInterface {
public:
    TsMuxer();
    virtual ~TsMuxer();
    void addTrack(const Track::Ptr &track) override;
    void resetTracks() override;
    void inputFrame(const Frame::Ptr &frame) override;
protected:
    virtual void onTs(const void *packet, int bytes,uint32_t timestamp,bool is_idr_fast_packet) = 0;
private:
    void init();
    void uninit();
private:
    void  *_context = nullptr;
    char *_tsbuf[188];
    uint32_t _timestamp = 0;

    struct track_info{
        int track_id = -1;
        Stamp stamp;
    };
    unordered_map<int,track_info> _codec_to_trackid;
    List<Frame::Ptr> _frameCached;
    bool _is_idr_fast_packet = false;
    bool _have_video = false;
};

}//namespace mediakit
#endif //TSMUXER_H
