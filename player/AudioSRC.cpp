/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Util/logger.h"
#include "AudioSRC.h"
#include "SDLAudioDevice.h"

using namespace std;
using namespace toolkit;

AudioSRC::AudioSRC(AudioSRCDelegate *del) {
    _delegate = del;
}

AudioSRC::~AudioSRC() {}

void AudioSRC::setOutputAudioConfig(const SDL_AudioSpec &cfg) {
    int freq = _delegate->getPCMSampleRate();
    int format = _delegate->getPCMFormat();
    int channels = _delegate->getPCMChannel();
    if (-1 == SDL_BuildAudioCVT(&_audio_cvt, format, channels, freq, cfg.format, cfg.channels, cfg.freq)) {
        throw std::runtime_error("the format conversion is not supported");
    }
    InfoL << "audio cvt origin format, freq:" << freq << ", format:" << hex << format  << dec << ", channels:" << channels;
    InfoL << "audio cvt info, "
          << "needed:" << (int)_audio_cvt.needed
          << ", src_format:" << hex << (SDL_AudioFormat)_audio_cvt.src_format
          << ", dst_format:" << (SDL_AudioFormat)_audio_cvt.dst_format << dec
          << ", rate_incr:" << (double)_audio_cvt.rate_incr
          << ", len_mult:" << (int)_audio_cvt.len_mult
          << ", len_ratio:" << (double)_audio_cvt.len_ratio;
}

void AudioSRC::setEnableMix(bool flag) {
    _enabled = flag;
}

int AudioSRC::getPCMData(char *buf, int size) {
    if (!_enabled) {
        return 0;
    }
    if (!_audio_cvt.needed) {
        //获取原始数据，不需要频率转换
        return _delegate->getPCMData(buf, size);
    }

    if ((int)(size / _audio_cvt.len_ratio) != _origin_size) {
        _origin_size = size / _audio_cvt.len_ratio;
        _origin_buf.reset(new char[(std::max)(_origin_size, size)], [](char *ptr) {
            delete[] ptr;
        });
        InfoL << "origin pcm buffer size is:" << _origin_size << ", target pcm buffer size is:" << size;
    }

    auto origin_size = _delegate->getPCMData(_origin_buf.get(), _origin_size);
    if (!origin_size) {
        //获取数据失败
        TraceL << "get empty pcm data";
        return 0;
    }

    _audio_cvt.buf = (Uint8 *) _origin_buf.get();
    _audio_cvt.len = origin_size;
    if (0 != SDL_ConvertAudio(&_audio_cvt)) {
        WarnL << "SDL_ConvertAudio failed!";
        _audio_cvt.len_cvt = 0;
    }
    if (_audio_cvt.len_cvt) {
        _target_buf.append(_origin_buf.get(), _audio_cvt.len_cvt);
    }
    if (_target_buf.size() < size) {
        return 0;
    }
    memcpy(buf, _target_buf.data(), size);
    _target_buf.erase(0, size);
    return size;
}

////////////////////////////////////////////////////////////////////////

AudioPlayer::AudioPlayer() : AudioSRC(this) {
    _device = SDLAudioDevice::Instance().shared_from_this();
}

AudioPlayer::~AudioPlayer() {
    _device->delChannel(this);
}

void AudioPlayer::setup(int sample_rate, int channel, SDL_AudioFormat format) {
    _sample_rate = sample_rate;
    _channel = channel;
    _format = format;
    _device->addChannel(this);
}

SDL_AudioFormat AudioPlayer::getPCMFormat() {
    return _format;
}

int AudioPlayer::getPCMSampleRate() {
    return _sample_rate;
}

int AudioPlayer::getPCMChannel() {
    return _channel;
}

int AudioPlayer::getPCMData(char *buf, int size) {
    lock_guard<mutex> lck(_mtx);
    if (_buffer.size() < size) {
        return 0;
    }
    memcpy(buf, _buffer.data(), size);
    _buffer.erase(0, size);
    return size;
}

void AudioPlayer::playPCM(const char *data, size_t size) {
    lock_guard<mutex> lck(_mtx);
    _buffer.append(data, size);
}
