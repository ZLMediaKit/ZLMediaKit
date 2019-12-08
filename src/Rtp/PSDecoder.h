/*
 * MIT License
 *
 * Copyright (c) 2019 Gemfield <gemfield@civilnet.cn>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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

#ifndef ZLMEDIAKIT_PSDECODER_H
#define ZLMEDIAKIT_PSDECODER_H

#if defined(ENABLE_RTPPROXY)
#include <stdint.h>

namespace mediakit{

class PSDecoder {
public:
    PSDecoder();
    virtual ~PSDecoder();
    int decodePS(const uint8_t *data, int bytes);
protected:
    virtual void onPSDecode(int stream,
                            int codecid,
                            int flags,
                            int64_t pts,
                            int64_t dts,
                            const void *data,
                            int bytes) = 0;
private:
    void *_ps_demuxer = nullptr;
};

}//namespace mediakit

#endif//defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_PSDECODER_H
