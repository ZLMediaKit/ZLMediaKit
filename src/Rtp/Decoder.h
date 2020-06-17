/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_DECODER_H
#define ZLMEDIAKIT_DECODER_H

#include <stdint.h>
#include <memory>
#include <functional>
#include "Decoder.h"
#include "Common/MediaSink.h"

using namespace std;
namespace mediakit {

class Decoder {
public:
    typedef std::shared_ptr<Decoder> Ptr;
    typedef std::function<void(int stream,int codecid,int flags,int64_t pts,int64_t dts,const void *data,int bytes)> onDecode;
    virtual int input(const uint8_t *data, int bytes) = 0;
    virtual void setOnDecode(const onDecode &decode) = 0;
protected:
    Decoder() = default;
    virtual ~Decoder() = default;
};

/**
 * 合并一些时间戳相同的frame
 */
class FrameMerger {
public:
    FrameMerger() = default;
    ~FrameMerger() = default;
    void inputFrame(const Frame::Ptr &frame,const function<void(uint32_t dts,uint32_t pts,const Buffer::Ptr &buffer)> &cb);
private:
    List<Frame::Ptr> _frameCached;
};

class DecoderImp{
public:
    typedef enum {
        decoder_ts = 0,
        decoder_ps
    }Type;

    typedef std::shared_ptr<DecoderImp> Ptr;
    ~DecoderImp() = default;

    static Ptr createDecoder(Type type, MediaSinkInterface *sink);
    int input(const uint8_t *data, int bytes);

protected:
    void onTrack(const Track::Ptr &track);
    void onFrame(const Frame::Ptr &frame);

private:
    DecoderImp(const Decoder::Ptr &decoder, MediaSinkInterface *sink);
    void onDecode(int stream,int codecid,int flags,int64_t pts,int64_t dts,const void *data,int bytes);

private:
    Decoder::Ptr _decoder;
    MediaSinkInterface *_sink;
    FrameMerger _merger;
    int _codecid_video = 0;
    int _codecid_audio = 0;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_DECODER_H
