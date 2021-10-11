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

INSTANCE_IMP(SDLAudioDevice);

SDLAudioDevice::~SDLAudioDevice() {
    SDL_CloseAudio();
}

SDLAudioDevice::SDLAudioDevice() {
    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = DEFAULT_SAMPLERATE;
    wanted_spec.format = DEFAULT_FORMAT;
    wanted_spec.channels = DEFAULT_CHANNEL;
    wanted_spec.silence = 0;
    wanted_spec.samples = DEFAULT_SAMPLES;
    wanted_spec.userdata = this;
    wanted_spec.callback = [](void *userdata, Uint8 *stream, int len) {
        SDLAudioDevice *_this = (SDLAudioDevice *) userdata;
        _this->onReqPCM((char *) stream, len);
    };
    if (SDL_OpenAudio(&wanted_spec, &_audio_config) < 0) {
        throw std::runtime_error("SDL_OpenAudio failed");
    }

    InfoL << "actual audioSpec, " << "freq:" << _audio_config.freq
          << ", format:" << hex << _audio_config.format << dec
          << ", channels:" << (int) _audio_config.channels
          << ", samples:" << _audio_config.samples
          << ", pcm size:" << _audio_config.size;

    _play_buf.reset(new char[_audio_config.size], [](char *ptr) {
        delete[] ptr;
    });
}

void SDLAudioDevice::addChannel(AudioSRC *chn) {
    lock_guard<recursive_mutex> lck(_channel_mtx);
    if (_channels.empty()) {
        SDL_PauseAudio(0);
    }
    chn->setOutputAudioConfig(_audio_config);
    _channels.emplace(chn);
}

void SDLAudioDevice::delChannel(AudioSRC *chn) {
    lock_guard<recursive_mutex> lck(_channel_mtx);
    _channels.erase(chn);
    if (_channels.empty()) {
        SDL_PauseAudio(true);
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

