/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIOSRC_H_
#define AUDIOSRC_H_

#include <memory>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif
#include "SDL2/SDL.h"
#ifdef __cplusplus
}
#endif

#if defined(_WIN32)
#pragma comment(lib,"SDL2.lib")
#endif //defined(_WIN32)

#include "Network/Buffer.h"
#include "SDLAudioDevice.h"

class AudioSRCDelegate {
public:
    virtual ~AudioSRCDelegate() {};
    virtual SDL_AudioFormat getPCMFormat() = 0;
    virtual int getPCMSampleRate() = 0;
    virtual int getPCMChannel() = 0;
    virtual int getPCMData(char *buf, int size) = 0;
};

//该类实现pcm的重采样
class AudioSRC {
public:
    typedef std::shared_ptr<AudioSRC> Ptr;
    AudioSRC(AudioSRCDelegate *);
    virtual ~AudioSRC();

    void setEnableMix(bool flag);
    void setOutputAudioConfig(const SDL_AudioSpec &cfg);
    int getPCMData(char *buf, int size);

private:
    bool _enabled = true;
    int _buf_size = 0;
    std::shared_ptr<char> _buf;
    AudioSRCDelegate *_delegate = nullptr;
    toolkit::BufferLikeString _target_buf;
    SDL_AudioCVT _audio_cvt;
};

class AudioPlayer : public AudioSRC, private AudioSRCDelegate{
public:
    AudioPlayer();
    ~AudioPlayer() override;

    void setup(int sample_rate, int channel, SDL_AudioFormat format);
    void playPCM(const char *data, size_t size);

private:
    SDL_AudioFormat getPCMFormat() override;
    int getPCMSampleRate() override;
    int getPCMChannel() override;
    int getPCMData(char *buf, int size) override;

private:
    int _sample_rate, _channel;
    SDL_AudioFormat _format;
    std::mutex _mtx;
    toolkit::BufferLikeString _buffer;
    SDLAudioDevice::Ptr _device;
};

#endif /* AUDIOSRC_H_ */
