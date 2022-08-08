/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_TRANSCODE_H
#define ZLMEDIAKIT_TRANSCODE_H

#if defined(ENABLE_FFMPEG)

#include "Util/TimeTicker.h"
#include "Common/MediaSink.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "libswscale/swscale.h"
#include "libavutil/avutil.h"
#include "libavutil/pixdesc.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/imgutils.h"
#ifdef __cplusplus
}
#endif

namespace mediakit {

class FFmpegFrame {
public:
    using Ptr = std::shared_ptr<FFmpegFrame>;

    FFmpegFrame(std::shared_ptr<AVFrame> frame = nullptr);
    ~FFmpegFrame();

    AVFrame *get() const;
    void fillPicture(AVPixelFormat target_format, int target_width, int target_height);

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
    virtual ~TaskManager();

    void setMaxTaskSize(size_t size);
    void stopThread(bool drop_task);

protected:
    void startThread(const std::string &name);
    bool addEncodeTask(std::function<void()> task);
    bool addDecodeTask(bool key_frame, std::function<void()> task);
    bool isEnabled() const;

private:
    void onThreadRun(const std::string &name);

private:
    class ThreadExitException : public std::runtime_error {
    public:
        ThreadExitException() : std::runtime_error("exit") {}
        ~ThreadExitException() = default;
    };

private:
    bool _decode_drop_start = false;
    bool _exit = false;
    size_t _max_task = 30;
    std::mutex _task_mtx;
    toolkit::semaphore _sem;
    toolkit::List<std::function<void()> > _task;
    std::shared_ptr<std::thread> _thread;
};

class FFmpegDecoder : public TaskManager {
public:
    using Ptr = std::shared_ptr<FFmpegDecoder>;
    using onDec = std::function<void(const FFmpegFrame::Ptr &)>;

    FFmpegDecoder(const Track::Ptr &track, int thread_num = 2);
    ~FFmpegDecoder() override;

    bool inputFrame(const Frame::Ptr &frame, bool live, bool async, bool enable_merge = true);
    void setOnDecode(onDec cb);
    void flush();
    const AVCodecContext *getContext() const;

private:
    void onDecode(const FFmpegFrame::Ptr &frame);
    bool inputFrame_l(const Frame::Ptr &frame, bool live, bool enable_merge);
    bool decodeFrame(const char *data, size_t size, uint64_t dts, uint64_t pts, bool live);

private:
    bool _do_merger = false;
    toolkit::Ticker _ticker;
    onDec _cb;
    std::shared_ptr<AVCodecContext> _context;
    FrameMerger _merger{FrameMerger::h264_prefix};
};

class FFmpegSws {
public:
    using Ptr = std::shared_ptr<FFmpegSws>;

    FFmpegSws(AVPixelFormat output, int width, int height);
    ~FFmpegSws();
    FFmpegFrame::Ptr inputFrame(const FFmpegFrame::Ptr &frame);
    int inputFrame(const FFmpegFrame::Ptr &frame, uint8_t *data);

private:
    int _target_width;
    int _target_height;
    SwsContext *_ctx = nullptr;
    AVPixelFormat _target_format;
};

}//namespace mediakit
#endif// ENABLE_FFMPEG
#endif //ZLMEDIAKIT_TRANSCODE_H
