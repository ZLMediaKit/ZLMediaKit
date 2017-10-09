/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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


#ifndef CODEC_AACENCODER_H_
#define CODEC_AACENCODER_H_

namespace ZL {
namespace Codec {

class AACEncoder {
public:
	AACEncoder(void);
	virtual ~AACEncoder(void);
	bool init(int iSampleRate, int iAudioChannel, int iAudioSampleBit);
	int inputData(char *pcData, int iLen, unsigned char **ppucOutBuffer);

private:
	unsigned char *m_pucPcmBuf = nullptr;
	unsigned int m_uiPcmLen = 0;

	unsigned char *m_pucAacBuf = nullptr;
	void *m_hEncoder = nullptr;

	unsigned long m_ulInputSamples = 0;
	unsigned long m_ulMaxInputBytes = 0;
	unsigned long m_ulMaxOutputBytes = 0;

};

} /* namespace Codec */
} /* namespace ZL */

#endif /* CODEC_AACENCODER_H_ */
