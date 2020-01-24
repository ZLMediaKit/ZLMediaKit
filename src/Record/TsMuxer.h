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
