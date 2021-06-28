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

#ifndef SDLAUDIOMIXER_AUDIOSRC_H_
#define SDLAUDIOMIXER_AUDIOSRC_H_

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

using namespace std;


class AudioSRCDelegate{
public:
	virtual ~AudioSRCDelegate(){};
	virtual void setPCMBufferSize(int bufsize) = 0;
	virtual int getPCMSampleBit() = 0;
	virtual int getPCMSampleRate() = 0;
	virtual int getPCMChannel() = 0;
	virtual int getPCMData(char *buf, int bufsize) = 0;
};


class AudioSRC
{
public:
	typedef std::shared_ptr<AudioSRC> Ptr;
	AudioSRC(AudioSRCDelegate *);
	virtual ~AudioSRC();
	void setEnableMix(bool flag);
	void setOutputAudioConfig(const SDL_AudioSpec &cfg);
	//此处buf大小务必要比bufsize大的多
	int getPCMData(char *buf, int bufsize);
private:
	AudioSRCDelegate *_delegate;
	SDL_AudioCVT _audioCvt;
	bool _enableMix = true;
	int _pcmSize = 0;
	string _pcmBuf;
};
#endif /* SDLAUDIOMIXER_AUDIOSRC_H_ */
