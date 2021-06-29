/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef FFMpegDecoder_H_
#define FFMpegDecoder_H_
#include <string>
#include <memory>
#include <stdexcept>
#include "Extension/Frame.h"
#include "Extension/Track.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#ifdef __cplusplus
}
#endif

using namespace std;
using namespace mediakit;

class FFmpegFrame {
public:
    using Ptr = std::shared_ptr<FFmpegFrame>;

    FFmpegFrame(std::shared_ptr<AVFrame> frame = nullptr);
    ~FFmpegFrame();

    AVFrame *get() const;
    void fillPicture(AVPixelFormat target_format, int target_width, int  target_height);

private:
    char *_data = nullptr;
    std::shared_ptr<AVFrame> _frame;
};

class FFmpegSwr{
public:
    using Ptr = std::shared_ptr<FFmpegSwr>;

    FFmpegSwr(AVSampleFormat output, int channel, int channel_layout, int samplerate);
    ~FFmpegSwr();

    FFmpegFrame::Ptr inputFrame(const FFmpegFrame::Ptr &frame);

private:
    int _target_channels;
    int _target_channel_layout;
    int _target_samplerate;
    AVSampleFormat _target_format;
    SwrContext *_ctx = nullptr;
    ResourcePool<FFmpegFrame> _frame_pool;
};

class FFmpegDecoder : public FrameWriterInterface {
public:
    using Ptr = std::shared_ptr<FFmpegDecoder>;
    using onDec = function<void(const FFmpegFrame::Ptr &)>;

    FFmpegDecoder(const Track::Ptr &track);
    ~FFmpegDecoder() {}

    void inputFrame(const Frame::Ptr &frame) override;
    void inputFrame(const char *data, size_t size, uint32_t dts, uint32_t pts);

    void setOnDecode(onDec cb);
    void flush();
    const AVCodecContext *getContext() const;

private:
    void onDecode(const FFmpegFrame::Ptr &frame);

private:
    Ticker _ticker;
    onDec _cb;
    FFmpegSwr::Ptr _swr;
    ResourcePool<FFmpegFrame> _frame_pool;
    std::shared_ptr<AVCodecContext> _context;
};

#endif /* FFMpegDecoder_H_ */


