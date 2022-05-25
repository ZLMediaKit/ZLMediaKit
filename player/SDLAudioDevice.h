/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDLAUDIOMIXER_SDLAUDIODEVICE_H_
#define SDLAUDIOMIXER_SDLAUDIODEVICE_H_

#include <mutex>
#include <memory>
#include <stdexcept>
#include <unordered_set>

#define DEFAULT_SAMPLERATE 48000
#define DEFAULT_FORMAT AUDIO_S16
#define DEFAULT_CHANNEL 2
#define DEFAULT_SAMPLES 2048


class AudioSRC;

//该对象主要实现sdl混音与播放
class SDLAudioDevice : public std::enable_shared_from_this<SDLAudioDevice>{
public:
    using Ptr = std::shared_ptr<SDLAudioDevice>;

    ~SDLAudioDevice();
    static SDLAudioDevice &Instance();

    void addChannel(AudioSRC *chn);
    void delChannel(AudioSRC *chn);

private:
    SDLAudioDevice();
    void onReqPCM(char *stream, int len);

private:
    std::shared_ptr<char> _play_buf;
    SDL_AudioSpec _audio_config;
    std::recursive_mutex _channel_mtx;
    std::unordered_set<AudioSRC *> _channels;
};

#endif /* SDLAUDIOMIXER_SDLAUDIODEVICE_H_ */
