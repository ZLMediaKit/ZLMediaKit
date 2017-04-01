/*
 * AACEncoder.h
 *
 *  Created on: 2016年8月11日
 *      Author: xzl
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
