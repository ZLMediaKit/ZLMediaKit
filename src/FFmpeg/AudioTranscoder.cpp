/*
* Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
*
* This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
*
* Use of this source code is governed by MIT license that can be found in the
* LICENSE file in the root of the source tree. All contributing project authors
* may be found in the AUTHORS file in the root of the source tree.
*/
#if defined(ENABLE_FFMPEG)
#include "AudioTranscoder.h"

using namespace toolkit;
namespace mediakit {

static int getSampleRateIndex(int sample_rate) {
    static int sample_rates[] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350 };
    int num_sample_rates = sizeof(sample_rates) / sizeof(sample_rates[0]);

    for (int i = 0; i < num_sample_rates; i++) {
        if (sample_rate == sample_rates[i]) {
            return i;
        }
    }
    // 如果没有找到，返回0
    // Default to index 0 if sample rate is not found
    return 0;
}
static uint8_t *addADTSHeader(AVPacket *packet, int sample_rate, int channel_count) {
    int adts_size = 7;
    int packet_size = packet->size + adts_size;
    auto new_data = (uint8_t *)av_malloc(packet_size);

    // ADTS header
    int profile = FF_PROFILE_AAC_LOW; // AAC LC
    int freq_index = getSampleRateIndex(sample_rate);
    int channel_config = channel_count;

    new_data[0] = 0xFF; // syncword
    new_data[1] = 0xF1; // MPEG-4, AAC-LC
    new_data[2] = (profile << 6) | (freq_index << 2) | (channel_config >> 2);
    new_data[3] = ((channel_config & 3) << 6) | (packet_size >> 11);
    new_data[4] = (packet_size >> 3) & 0xFF;
    new_data[5] = ((packet_size & 7) << 5) | 0x1F;
    new_data[6] = 0xFC;

    // 复制ADTS头后的原始包数据
    // Copy the original packet data after the ADTS header
    memcpy(new_data + adts_size, packet->data, packet->size);
    return new_data;
}

AudioTranscoder::~AudioTranscoder() {
    cleanup();
}

void AudioTranscoder::inputAudioData(const uint8_t *data, size_t size, int64_t pts, int64_t dts) {
    if (_release || !_decoder_ctx) {
        return;
    }
    auto packet = ffmpeg::alloc_av_packet();
    packet->data = const_cast<uint8_t *>(data);
    packet->size = (int)size;

    int ret = avcodec_send_packet(_decoder_ctx.get(), packet.get());
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN)) {
            // Decoder is not ready to accept new data, try again later
            ErrorL << "Decoder not ready to accept new data, try again later";
            return;
        } else if (ret == AVERROR_EOF) {
            // 输入流结束，刷新解码器
            // End of input stream, flush the decoder
            avcodec_send_packet(_decoder_ctx.get(), nullptr);
        } else {
            ErrorL << "Failed to send packet to decoder, error: " << ret;
            return;
        }
    }

    while (ret >= 0 && !_release) {
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(_decoder_ctx.get(), frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_frame_free(&frame);
            break;
        } else if (ret < 0) {
            ErrorL << "Error receiving frame from decoder: " << ret;
            av_frame_free(&frame);
            return;
        }
        if (!_encoder_ctx) {
            if (!initEncodeCodec(
                    frame->sample_rate, frame->ch_layout.nb_channels, _decoder_ctx->bit_rate, AV_CODEC_ID_AAC)) {
                ErrorL << "Failed to init encode codec";
                return;
            }
        }
        transcodeFrame(frame, pts, dts);
    }
}

// 销毁所有资源
// Destroy all resources
void AudioTranscoder::release() {
    _release = true;
    _callback = nullptr;
}

void AudioTranscoder::cleanup() {
    _callback = nullptr;
    av_channel_layout_uninit(&_audio_params_ch_layout);
    if (_decoder_ctx) {
        _decoder_ctx.reset();
    }
    if (_encoder_ctx) {
        _encoder_ctx.reset();
    }
    if (_swr_ctx) {
        _swr_ctx.reset();
    }
    if (_fifo) {
        _fifo.reset();
    }
}
int AudioTranscoder::init_re_sampler(AVFrame *pFrame) {
    int ret = 0;
    if (pFrame->format != _audio_params_fmt|| pFrame->sample_rate != _audio_params_freq
        || av_channel_layout_compare(&pFrame->ch_layout, &_audio_params_ch_layout) || !_swr_ctx) {
        _swr_ctx.reset();
        InfoL << "init resample context";
        SwrContext *resample_context = nullptr;
        ret = swr_alloc_set_opts2(
            &resample_context, &_encoder_ctx->ch_layout, _encoder_ctx->sample_fmt, _encoder_ctx->sample_rate, &_decoder_ctx->ch_layout,
            _decoder_ctx->sample_fmt, _decoder_ctx->sample_rate, 0, nullptr);
        if (ret < 0) {
            ErrorL << "Failed to allocate resample context: " << ret;
            return ret;
        }
        // 打开重采样器
        // Open the resampler with the specified parameters.
        if ((ret = swr_init(resample_context)) < 0) {
            ErrorL << "Could not open resample context: " << ret;
            swr_free(&resample_context);
            return ret;
        }
        if (av_channel_layout_copy(&_audio_params_ch_layout, &pFrame->ch_layout) < 0) {
            swr_free(&resample_context);
            ErrorL << "Failed to copy channel layout";
            return -1;
        }
        _audio_params_freq = pFrame->sample_rate;
        _audio_params_fmt= pFrame->format;
        _swr_ctx.reset(resample_context);
    }
    for (int i = 0; i < _audio_params_ch_layout.nb_channels; i++) {
        if (!pFrame->extended_data[i]) {
            ErrorL << "extended_data[" << i << "] is null";
            return -1;
        }
    }
    return ret;
}

bool AudioTranscoder::initDecodeCodec(int psi_codec_id) {
    AVCodecID codec_id = ffmpeg::psi_to_avcodec_id(psi_codec_id);
    if (codec_id == AV_CODEC_ID_NONE) {
        ErrorL << "Codec ID not found for PSI codec ID: " << psi_codec_id;
        return false;
    }
    if (_decoder_ctx && _decoder_ctx->codec_id == codec_id) {
        return true;
    }
    const AVCodec *codec = avcodec_find_decoder(codec_id);
    if (!codec) {
        ErrorL << "Decoder not found for codec ID: " << codec_id;
        return false;
    }
    _decoder_ctx.reset(avcodec_alloc_context3(codec));
    if (!_decoder_ctx) {
        ErrorL << "Failed to allocate decoder context";
        return false;
    }

    if (avcodec_open2(_decoder_ctx.get(), codec, nullptr) < 0) {
        ErrorL << "Failed to open decoder";
        return false;
    }

    return true;
}
bool AudioTranscoder::initEncodeCodec(int sample_rate, int channels, int bit_rate, AVCodecID codec_id) {
    if (_encoder_ctx && _encoder_ctx->codec_id == codec_id) {
        return true;
    }
    auto codec = avcodec_find_encoder(codec_id);
    if (!codec) {
        ErrorL << "Encoder not found for codec ID: " << codec_id;
        return false;
    }
    _encoder_ctx.reset(avcodec_alloc_context3(codec));
    if (!_encoder_ctx) {
        ErrorL << "Failed to allocate encoder context";
        return false;
    }
    _encoder_ctx->sample_rate = sample_rate;
    _encoder_ctx->bit_rate = bit_rate;
//    _encoder_ctx->sample_fmt = codec->sample_fmts[0];
    _encoder_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    _encoder_ctx->time_base = (AVRational) { 1, sample_rate };
    _encoder_ctx->profile = FF_PROFILE_AAC_LOW;
    av_channel_layout_default(&_encoder_ctx->ch_layout, channels);

    if (avcodec_open2(_encoder_ctx.get(), codec, nullptr) < 0) {
        ErrorL << "Failed to open encoder";
        return false;
    }
    return true;
}
void AudioTranscoder::transcodeFrame(AVFrame *inputFrame, int64_t pts, int64_t dts) {
    if (_audio_params_freq <= 0) {
        if (av_channel_layout_copy(&_audio_params_ch_layout, &_decoder_ctx->ch_layout) < 0) {
            return;
        }
        _audio_params_freq = _decoder_ctx->sample_rate;
        _audio_params_fmt= _decoder_ctx->sample_fmt;
    }
    int ret = 0;
    if (inputFrame->format != _audio_params_fmt || inputFrame->sample_rate != _audio_params_freq
        || av_channel_layout_compare(&inputFrame->ch_layout, &_audio_params_ch_layout) || !_fifo) {
        _fifo.reset(av_audio_fifo_alloc(_encoder_ctx->sample_fmt, _encoder_ctx->ch_layout.nb_channels, 1));
        InfoL << "init fifo";
        if (!_fifo) {
            ErrorL << "Failed to allocate FIFO";
            return;
        }
    }
    if (init_re_sampler(inputFrame) < 0) {
        ErrorL << "Failed to init resampler";
        return;
    }
    uint8_t **converted_input_samples = nullptr;
    ret = av_samples_alloc_array_and_samples(
        &converted_input_samples, nullptr, _encoder_ctx->ch_layout.nb_channels, inputFrame->nb_samples, _encoder_ctx->sample_fmt, 0);
    if (ret < 0) {
        ErrorL << "Could not allocate converted input samples, error: " << ret;
        return;
    }
    // converted_input_samples is used as a temporary storage for the converted input samples
    std::unique_ptr<uint8_t *, std::function<void(uint8_t **)>> converted_input_samples_ptr(converted_input_samples, [](uint8_t **p) {
        if (p) {
            av_freep(&p[0]);
            av_freep(&p);
        }
    });
    // 转换输入样本为期望的输出样本格式
    // 这需要一个由converted_input_samples提供的临时存储
    // Convert the input samples to the desired output sample format.
    // This requires a temporary storage provided by converted_input_samples.
    ret = swr_convert(
        _swr_ctx.get(), converted_input_samples_ptr.get(), inputFrame->nb_samples, (const uint8_t **)inputFrame->extended_data, inputFrame->nb_samples);

    if (ret < 0) {
        ErrorL << "Could not convert input samples";
        converted_input_samples_ptr.reset();
        return;
    }
    if (av_audio_fifo_realloc(_fifo.get(), av_audio_fifo_size(_fifo.get()) + inputFrame->nb_samples) < 0) {
        ErrorL << "Could not reallocate FIFO";
        converted_input_samples_ptr.reset();
        return;
    }
    if (av_audio_fifo_write(_fifo.get(), (void **)converted_input_samples_ptr.get(), inputFrame->nb_samples) < inputFrame->nb_samples) {
        ErrorL << "Could not write data to FIFO";
        converted_input_samples_ptr.reset();
        return;
    }
    converted_input_samples_ptr.reset();
    av_frame_free(&inputFrame);
    const int output_frame_size = _encoder_ctx->frame_size;
    // 读取FIFO缓冲区中的数据并将其传递给回调函数
    // Read data from FIFO buffer and pass it to the callback function
    while (av_audio_fifo_size(_fifo.get()) >= output_frame_size) {
        AVFrame *outputFrame = av_frame_alloc();
        if (!outputFrame) {
            ErrorL << "Failed to allocate output frame";
            return;
        }
        const int frame_size = FFMIN(av_audio_fifo_size(_fifo.get()), output_frame_size);
        outputFrame->nb_samples = frame_size;
        av_channel_layout_copy(&outputFrame->ch_layout, &_encoder_ctx->ch_layout);
        outputFrame->format = _encoder_ctx->sample_fmt;
        outputFrame->sample_rate = _encoder_ctx->sample_rate;
        ret = av_frame_get_buffer(outputFrame, 0);
        if (ret < 0) {
            ErrorL << "Failed to allocate output frame samples: " << ret;
            av_frame_free(&outputFrame);
            return;
        }
        ret = av_audio_fifo_read(_fifo.get(), (void **)outputFrame->data, frame_size);
        if (ret < frame_size) {
            ErrorL << "Failed to read audio data from FIFO buffer: " << ret;
            av_frame_free(&outputFrame);
            return;
        }
        if (avcodec_send_frame(_encoder_ctx.get(), outputFrame) < 0) {
            ErrorL << "Failed to send frame to encoder";
            av_frame_free(&outputFrame);
            return;
        }
        AVPacket *output_packet = av_packet_alloc();
        ret = avcodec_receive_packet(_encoder_ctx.get(), output_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_free(&output_packet);
            av_frame_free(&outputFrame);
            break;
        } else if (ret < 0) {
            ErrorL << "Error receiving packet from encoder: " << ret;
            av_packet_free(&output_packet);
            av_frame_free(&outputFrame);
            return;
        }
        auto outBuff = addADTSHeader(output_packet, _encoder_ctx->sample_rate, _encoder_ctx->ch_layout.nb_channels);
        onOutputAudioData(outBuff, output_packet->size + 7, pts, dts);
        av_freep(&outBuff);
        av_packet_free(&output_packet);
        av_frame_free(&outputFrame);
    }
}
void AudioTranscoder::onOutputAudioData(const uint8_t *data, int size, int64_t pts, int64_t dts) {
    if (_callback) {
        _callback(data, size, pts, dts);
    }
}
void AudioTranscoder::setOnOutputAudioData(const AudioTranscoder::onTranscodeCallback &cb) {
    _callback = cb;
}
} // namespace mediakit
#endif // defined(ENABLE_FFMPEG)