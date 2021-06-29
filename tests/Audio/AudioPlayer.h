/* MIT License
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

#ifndef AUDIO_AUDIOPLAYER_H
#define AUDIO_AUDIOPLAYER_H

#include "SDLAudioMixer/AudioSRC.h"
#include "SDLAudioMixer/SDLAudioDevice.h"
#include "AudioDecoder.h"
#include "xRingBuffer.h"

#include "Extension/Frame.h"

using mediakit::Frame;

class PcmPacket{
public:
    string data;
    uint32_t timeStmp;
};


class AudioPlayer:public AudioSRCDelegate{
public:
    AudioPlayer( int sampleRate ,int  channels ,int  sampleBit);
    ~AudioPlayer() override;

    //audio
    void setPCMBufferSize(int size) override{
        InfoL << "setPCMBufferSize = " << size ;
        reqSize_ = size;
    }
    int getPCMSampleBit() override{
        return decoder_->getSampleBit();
    }
    int getPCMSampleRate() override{
        return decoder_->getSampleRate();
    }
    int getPCMChannel() override{
        return decoder_->getChannels();
    }

    int getPCMData(char *buf, int bufsize) override;

    bool inputFrame( Frame::Ptr frame );

private:
    int                             sampleRate_;
    int                             channels_;
    int                             sampleBit_;

    string                          buf_;

    int                             reqSize_ = 4096;
    AudioDecoder*                   decoder_=nullptr;
    xRingBuffer<PcmPacket>*         buffer_ = nullptr;
    AudioSRC *                      audioSrc_ = nullptr;
};

#endif //AUDIO_AUDIOPLAYER_H
