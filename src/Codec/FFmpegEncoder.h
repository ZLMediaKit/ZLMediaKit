/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_FFMPEGENCODER_H
#define ZLMEDIAKIT_FFMPEGENCODER_H

#if defined(ENABLE_FFMPEG)

#include "Transcode.h"
#include "Extension/Frame.h"
#include "Extension/Track.h"

namespace mediakit {

/**
 * FFmpeg编码器 - 支持多种视频和音频编解码器
 * 视频: H264, H265, VP8, VP9, AV1, SVAC, JPEG
 * 音频: AAC, Opus, G711A, G711U, G722, MP3
 */
class FFmpegEncoder : public TaskManager {
public:
    using Ptr = std::shared_ptr<FFmpegEncoder>;
    using onEnc = std::function<void(const Frame::Ptr &)>;

    /**
     * 构造函数
     * @param track 目标编码Track信息
     * @param thread_num 编码线程数
     * @param encoder_name 指定编码器名称列表（可选）
     */
    FFmpegEncoder(const Track::Ptr &track, int thread_num = 2, const std::vector<std::string> &encoder_name = {});
    ~FFmpegEncoder() override;

    /**
     * 输入解码后的帧进行编码
     * @param frame FFmpeg解码后的帧
     * @param async 是否异步编码
     */
    bool inputFrame(const FFmpegFrame::Ptr &frame, bool async = true);

    /**
     * 设置编码完成回调
     * @param cb 回调函数
     */
    void setOnEncode(onEnc cb);

    /**
     * 刷新编码器缓冲区
     */
    void flush();

    /**
     * 获取编码器上下文
     */
    const AVCodecContext *getContext() const;

    /**
     * 获取目标Track
     */
    Track::Ptr getTrack() const { return _target_track; }

private:
    bool inputFrame_l(const FFmpegFrame::Ptr &frame);
    void onEncode(const AVPacket *pkt);
    Frame::Ptr makeFrame(const AVPacket *pkt);

private:
    bool _opened = false;
    onEnc _cb;
    CodecId _codec_id;
    Track::Ptr _target_track;
    std::shared_ptr<AVCodecContext> _context;
    FFmpegSwr::Ptr _swr;  // 音频重采样
    FFmpegSws::Ptr _sws;  // 视频缩放
    toolkit::ResourcePool<FFmpegFrame> _frame_pool;
};

/**
 * 音频编码器配置
 */
struct AudioEncoderConfig {
    CodecId codec_id = CodecAAC;
    int sample_rate = 44100;
    int channels = 2;
    int bit_rate = 128000;
    std::string encoder_name;  // 可选，指定编码器
};

/**
 * 视频编码器配置
 */
struct VideoEncoderConfig {
    CodecId codec_id = CodecH264;
    int width = 1920;
    int height = 1080;
    int fps = 25;
    int bit_rate = 2000000;
    int gop = 50;              // 关键帧间隔
    std::string encoder_name;  // 可选，指定编码器
    std::string preset = "medium";  // 编码预设
    std::string profile = "";       // 编码profile
};

/**
 * 创建音频编码器Track
 */
Track::Ptr createAudioTrack(const AudioEncoderConfig &config);

/**
 * 创建视频编码器Track
 */
Track::Ptr createVideoTrack(const VideoEncoderConfig &config);

/**
 * 获取CodecId对应的FFmpeg AVCodecID
 */
AVCodecID getAVCodecID(CodecId codec_id);

/**
 * 获取FFmpeg AVCodecID对应的CodecId
 */
CodecId getCodecId(AVCodecID av_codec_id);

} // namespace mediakit

#endif // ENABLE_FFMPEG
#endif // ZLMEDIAKIT_FFMPEGENCODER_H
