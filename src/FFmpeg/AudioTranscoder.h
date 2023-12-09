/*
* Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
*
* This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
*
* Use of this source code is governed by MIT license that can be found in the
* LICENSE file in the root of the source tree. All contributing project authors
* may be found in the AUTHORS file in the root of the source tree.
*/

#ifndef ZLMEDIAKIT_AUDIOTRANSCODER_H
#define ZLMEDIAKIT_AUDIOTRANSCODER_H
#if defined(ENABLE_FFMPEG)
#include "FFmpeg/Utils.h"

namespace mediakit {
class AudioTranscoder {

public:
    using Ptr = std::shared_ptr<AudioTranscoder>;
    using onTranscodeCallback = std::function<void(const uint8_t *data, int size, int64_t pts, int64_t dts)>;

    AudioTranscoder() {
        _decoder_ctx = std::unique_ptr<AVCodecContext, std::function<void(AVCodecContext *)>>(nullptr, [](AVCodecContext *p) {
                  if (p) {
                      avcodec_free_context(&p);
                  }
              });
        _encoder_ctx = std::unique_ptr<AVCodecContext, std::function<void(AVCodecContext *)>>(nullptr, [](AVCodecContext *p) {
                  if (p) {
                      avcodec_free_context(&p);
                  }
              });
        _swr_ctx = std::unique_ptr<SwrContext, std::function<void(SwrContext *)>>(nullptr, [](SwrContext *p) {
            if (p) {
                swr_free(&p);
            }
        });
        _fifo = std::unique_ptr<AVAudioFifo, std::function<void(AVAudioFifo *)>>(nullptr, [](AVAudioFifo *p) {
            if (p) {
                av_audio_fifo_free(p);
            }
        });
    }
    ~AudioTranscoder();
    void setOnOutputAudioData(const onTranscodeCallback &cb);
    void onOutputAudioData(const uint8_t *data, int size, int64_t pts, int64_t dts);
    void inputAudioData(const uint8_t *data, size_t size, int64_t pts, int64_t dts);
    bool initDecodeCodec(int psi_codec_id);
    bool initEncodeCodec(int sample_rate, int channels, int bit_rate, AVCodecID codec_id=AV_CODEC_ID_AAC);
    void release();
protected:
    int init_re_sampler(AVFrame *pFrame);

private:
    bool _release = false;
    int _audio_params_freq = 0;
    int _audio_params_fmt = 0;
    AVChannelLayout _audio_params_ch_layout = {};
    std::unique_ptr<AVCodecContext, std::function<void(AVCodecContext *)>> _decoder_ctx;
    std::unique_ptr<AVCodecContext, std::function<void(AVCodecContext *)>> _encoder_ctx;
    std::unique_ptr<SwrContext, std::function<void(SwrContext *)>> _swr_ctx;
    std::unique_ptr<AVAudioFifo, std::function<void(AVAudioFifo *)>> _fifo;
    onTranscodeCallback _callback;
    void cleanup();
    void transcodeFrame(AVFrame *frame, int64_t i, int64_t i1);
};

} // namespace mediakit

#endif // defined(ENABLE_FFMPEG)
#endif // ZLMEDIAKIT_AUDIOTRANSCODER_H
