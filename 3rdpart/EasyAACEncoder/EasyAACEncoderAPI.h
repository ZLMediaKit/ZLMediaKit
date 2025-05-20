/*
	Copyright (c) 2013-2016 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.easydarwin.org
*/

#ifndef EasyAACEncoder_API_H
#define	EasyAACEncoder_API_H

#ifdef _WIN32
#define Easy_API  __declspec(dllexport)
#define Easy_APICALL  __stdcall
#else
#define Easy_API
#define Easy_APICALL 
#endif

#define EasyAACEncoder_Handle void*

///* Audio Codec */
enum Law
{
	Law_ULaw	=	0, 		/**< U law */
	Law_ALaw	=	1, 		/**< A law */
	Law_PCM16	=	2, 		/**< 16 bit uniform PCM values. 原始 pcm 数据 */  
	Law_G726	=	3		/**< G726 */
};

///* Rate Bits */
enum Rate
{
	Rate16kBits=2,	/**< 16k bits per second (2 bits per ADPCM sample) */
	Rate24kBits=3,	/**< 24k bits per second (3 bits per ADPCM sample) */
	Rate32kBits=4,	/**< 32k bits per second (4 bits per ADPCM sample) */
	Rate40kBits=5	/**< 40k bits per second (5 bits per ADPCM sample) */
};

typedef struct _g711param
{
	;
}G711Param;

typedef struct _g726param
{
	unsigned char ucRateBits;//Rate16kBits Rate24kBits Rate32kBits Rate40kBits
}G726Param;

typedef struct _initParam
{
	unsigned char	ucAudioCodec;			// Law_uLaw  Law_ALaw Law_PCM16 Law_G726
	unsigned char	ucAudioChannel;			//1
	unsigned int	u32AudioSamplerate;		//8000
	unsigned int	u32PCMBitSize;			//16
	union
	{
		G711Param g711param;
		G726Param g726param;
	};

}InitParam;

#ifdef __cplusplus
extern "C"
{
#endif
	/* 创建AAC Encoder 返回为句柄值 */
	Easy_API EasyAACEncoder_Handle Easy_APICALL Easy_AACEncoder_Init(InitParam initPar);

	/* 输入编码数据，返回编码后数据 */
	Easy_API int Easy_APICALL Easy_AACEncoder_Encode(EasyAACEncoder_Handle handle, unsigned char* inbuf, unsigned int inlen, unsigned char* outbuf, unsigned int* outlen);

	/* 释放AAC Encoder */
	Easy_API void Easy_APICALL Easy_AACEncoder_Release(EasyAACEncoder_Handle handle);

#ifdef __cplusplus
}
#endif

#endif	/* EasyAACEncoder_API_H */
