/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Util/logger.h"
#include "AudioSRC.h"
#include "SDLAudioDevice.h"

using namespace std;
using namespace toolkit;

INSTANCE_IMP(SDLAudioDevice);

SDLAudioDevice::~SDLAudioDevice() {
    if (_init_thread.joinable()) {
        _init_thread.join();
    }
    auto dev = _device.load();
    if (dev) {
        SDL_CloseAudioDevice(dev);
        _device = 0;
    }
}

SDLAudioDevice::SDLAudioDevice() {
    // 将 SDL_OpenAudioDevice 放到后台线程异步执行, 防止主线程(SDL事件循环)和 event poller 线程阻塞
    // 在Windows下没声卡/设备异常时，SDL_OpenAudioDevice()会阻塞
    // 在 Linux(PulseAudio/ALSA) 和 macOS(CoreAudio) 上通常秒回，几乎无开销
    _init_thread = std::thread([this]() {
        SDL_AudioSpec wanted_spec;
        wanted_spec.freq = DEFAULT_SAMPLERATE;
        wanted_spec.format = DEFAULT_FORMAT;
        wanted_spec.channels = DEFAULT_CHANNEL;
        wanted_spec.silence = 0;
        wanted_spec.samples = DEFAULT_SAMPLES;
        wanted_spec.userdata = this;
        wanted_spec.callback = [](void *userdata, Uint8 *stream, int len) {
            SDLAudioDevice *_this = (SDLAudioDevice *)userdata;
            _this->onReqPCM((char *)stream, len);
        };

        SDL_AudioSpec audio_config;
        auto dev = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &audio_config, 0);
        if (dev <= 0)
            dev = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &audio_config, SDL_AUDIO_ALLOW_ANY_CHANGE);
        if (dev <= 0) {
            WarnL << "SDL_OpenAudioDevice failed, audio output disabled: " << SDL_GetError();
            return;
        }

        InfoL << "actual audioSpec, " << "freq:" << audio_config.freq
              << ", format:" << hex << audio_config.format << dec
              << ", channels:" << (int)audio_config.channels
              << ", samples:" << audio_config.samples
              << ", pcm size:" << audio_config.size;

        // 先设置 _audio_config 和 _play_buf，再设置 _device (release)
        // 这样 addChannel 读到 _device != 0 时，_audio_config 和 _play_buf 已就绪
        _audio_config = audio_config;
        _play_buf.reset(new char[audio_config.size], [](char *ptr) {
            delete[] ptr;
        });
        _device.store(dev, std::memory_order_release);
    });
}

void SDLAudioDevice::addChannel(AudioSRC *chn) {
    auto dev = _device.load(std::memory_order_acquire);
    if (dev == 0) {
        throw std::runtime_error("SDL audio device not available (not ready or no audio device)");
    }
    lock_guard<recursive_mutex> lck(_channel_mtx);
    if (_channels.empty()) {
        SDL_PauseAudioDevice(dev, false);
    }
    chn->setOutputAudioConfig(_audio_config);
    _channels.emplace(chn);
}

void SDLAudioDevice::delChannel(AudioSRC *chn) {
    lock_guard<recursive_mutex> lck(_channel_mtx);
    _channels.erase(chn);
    auto dev = _device.load();
    if (_channels.empty() && dev) {
        SDL_PauseAudioDevice(dev, true);
    }
}

void SDLAudioDevice::onReqPCM(char *stream, int len) {
    lock_guard<recursive_mutex> lck(_channel_mtx);
    int size;
    int channel = 0;
    for (auto &chn : _channels) {
        if (channel == 0) {
            size = chn->getPCMData(_play_buf.get(), len);
            if (size) {
                memcpy(stream, _play_buf.get(), size);
            }
        } else {
            size = chn->getPCMData(_play_buf.get(), len);
            if (size) {
                SDL_MixAudio((Uint8 *) stream, (Uint8 *) _play_buf.get(), size, SDL_MIX_MAXVOLUME);
            }
        }
        if (size) {
            channel++;
        }
    }

    if (!channel) {
        memset(stream, 0, len);
    }
}

