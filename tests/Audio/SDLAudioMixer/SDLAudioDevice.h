/*
 * MIT License
 *
 * Copyright (c) 2017 xiongziliang <771730766@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef SDLAUDIOMIXER_SDLAUDIODEVICE_H_
#define SDLAUDIOMIXER_SDLAUDIODEVICE_H_

#include <mutex>
#include <memory>
#include <stdexcept>
#include <unordered_set>

#include "AudioSRC.h"

using namespace std;


#define DEFAULT_SAMPLERATE 48000
#define DEFAULT_SAMPLEBIT 16
#define DEFAULT_CHANNEL 2
#define DEFAULT_PCMSIZE 4096

class SDLAudioDevice{
public:
	void addChannel(AudioSRC *chn);
	void delChannel(AudioSRC *chn);
	static SDLAudioDevice &Instance();
	virtual ~SDLAudioDevice();

protected:
private:
	SDLAudioDevice();
	static void onReqPCM (void *userdata, Uint8 * stream,int len);
	void onReqPCM (char * stream,int len);

    unordered_set<AudioSRC *>   _channelSet;
    recursive_mutex             _mutexChannel;
    SDL_AudioSpec               _audioConfig;
    std::shared_ptr<char>       _pcmBuf;
};

#endif /* SDLAUDIOMIXER_SDLAUDIODEVICE_H_ */
