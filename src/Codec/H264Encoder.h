/*
 * H264Encoder.h
 *
 *  Created on: 2016年8月11日
 *      Author: xzl
 */

#ifndef CODEC_H264ENCODER_H_
#define CODEC_H264ENCODER_H_
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include <x264.h>

#ifdef __cplusplus
}

#endif //__cplusplus

namespace ZL {
namespace Codec {

class H264Encoder {
public:
	typedef struct {
		int iType;
		int iLength;
		uint8_t *pucData;
	} H264Frame;

	H264Encoder(void);
	virtual ~H264Encoder(void);
	bool init(int iWidth, int iHeight, int iFps);
	int inputData(char *apcYuv[3], int aiYuvLen[3], int64_t i64Pts, H264Frame **ppFrame);
private:
	x264_t* m_pX264Handle = nullptr;
	x264_picture_t* m_pPicIn = nullptr;
	x264_picture_t* m_pPicOut = nullptr;
	H264Frame m_aFrames[10];
};

} /* namespace Codec */
} /* namespace ZL */

#endif /* CODEC_H264ENCODER_H_ */
