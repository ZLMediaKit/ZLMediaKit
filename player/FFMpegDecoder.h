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

#include "Util/TimeTicker.h"
#include "Common/MediaSink.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#ifdef __cplusplus
}
#endif

class FFmpegFrame {
public:
    using Ptr = std::shared_ptr<FFmpegFrame>;

    FFmpegFrame(std::shared_ptr<AVFrame> frame = nullptr);
    ~FFmpegFrame();

    AVFrame *get() const;

private:
    char *_data = nullptr;
    std::shared_ptr<AVFrame> _frame;
};

class FFmpegSwr {
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
};

class TaskManager {
public:
    TaskManager() = default;
    ~TaskManager();

protected:
    void startThread(const string &name);
    void stopThread();

    void addEncodeTask(function<void()> task);
    void addDecodeTask(bool key_frame, function<void()> task);
    bool isEnabled() const;

private:
    void onThreadRun(const string &name);
    void pushExit();

private:
    class ThreadExitException : public std::runtime_error {
    public:
        ThreadExitException() : std::runtime_error("exit") {}
        ~ThreadExitException() = default;
    };

private:
    bool _decode_drop_start = false;
    bool _exit = false;
    mutex _task_mtx;
    semaphore _sem;
    List<function<void()> > _task;
    std::shared_ptr<thread> _thread;
};

class FFmpegDecoder : private TaskManager {
public:
    using Ptr = std::shared_ptr<FFmpegDecoder>;
    using onDec = function<void(const FFmpegFrame::Ptr &)>;

    FFmpegDecoder(const Track::Ptr &track);
    ~FFmpegDecoder();

    bool inputFrame(const Frame::Ptr &frame, bool may_async = true);
    void setOnDecode(onDec cb);
    void flush();
    const AVCodecContext *getContext() const;

private:
    void onDecode(const FFmpegFrame::Ptr &frame);
    bool inputFrame_l(const Frame::Ptr &frame);
    bool decodeFrame(const char *data, size_t size, uint32_t dts, uint32_t pts);

private:
    bool _do_merger = false;
    Ticker _ticker;
    onDec _cb;
    std::shared_ptr<AVCodecContext> _context;
    FrameMerger _merger{FrameMerger::h264_prefix};
};

#endif /* FFMpegDecoder_H_ */


