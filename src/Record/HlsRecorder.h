/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef HLSRECORDER_H
#define HLSRECORDER_H

#include "HlsMakerImp.h"
#include "TsMuxer.h"
namespace mediakit {

class HlsRecorder
#if defined(ENABLE_HLS)
: public TsMuxer
#endif
        {
public:
    typedef std::shared_ptr<HlsRecorder> Ptr;
    HlsRecorder(const string &m3u8_file, const string &params){
        GET_CONFIG(uint32_t,hlsNum,Hls::kSegmentNum);
        GET_CONFIG(uint32_t,hlsBufSize,Hls::kFileBufSize);
        GET_CONFIG(uint32_t,hlsDuration,Hls::kSegmentDuration);
        _hls = new HlsMakerImp(m3u8_file,params,hlsBufSize,hlsDuration,hlsNum);
    }
    ~HlsRecorder(){
        delete _hls;
    }
    void setMediaSource(const string &vhost, const string &app, const string &stream_id){
        _hls->setMediaSource(vhost, app, stream_id);
    }

    MediaSource::Ptr getMediaSource() const{
        return _hls->getMediaSource();
    }
#if defined(ENABLE_HLS)
protected:
    void onTs(const void *packet, int bytes,uint32_t timestamp,bool is_idr_fast_packet) override {
        _hls->inputData((char *)packet,bytes,timestamp, is_idr_fast_packet);
    };
#endif
private:
    HlsMakerImp *_hls;
};
}//namespace mediakit
#endif //HLSRECORDER_H
