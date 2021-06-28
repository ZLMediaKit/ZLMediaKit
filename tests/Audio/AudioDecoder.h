/*
 * MIT License
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

#ifndef AUDIO_AUDIODECODER_H
#define AUDIO_AUDIODECODER_H

#include "libFaad/faad.h"

class AudioDecoder {
public:
    AudioDecoder();

    virtual  ~AudioDecoder();


    bool init(unsigned char * ADTSHead ,int headLen =7);

    int  inputData( unsigned char * data,int len , unsigned char ** outBuffer );

    unsigned long getChannels() const { return channels_ ;}
    unsigned long getSampleRate() const { return samplerate_ ; }
    unsigned long getSampleBit() const { return samplebit_ ;}


private:

    NeAACDecHandle  handle_;
    unsigned long   samplerate_;
    unsigned char   channels_;
    unsigned char   samplebit_;
};


#endif //AUDIO_AUDIODECODER_H
