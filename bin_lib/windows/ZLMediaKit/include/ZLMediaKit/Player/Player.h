/*
 * Player.h
 *
 *  Created on: 2016年12月2日
 *      Author: xzl
 */

#ifndef SRC_PLAYER_PLAYER_H_
#define SRC_PLAYER_PLAYER_H_

#include <string>
using namespace std;

typedef struct {
	uint16_t sequence;
	uint32_t timeStamp;
	unsigned char type;
	string data;
} H264Frame;

//ADTS 头中相对有用的信息 采样率、声道数、帧长度
typedef struct {
	unsigned int syncword; //12 bslbf 同步字The bit string ‘1111 1111 1111’，说明一个ADTS帧的开始
	unsigned int id;        //1 bslbf   MPEG 标示符, 设置为1
	unsigned int layer;    //2 uimsbf Indicates which layer is used. Set to ‘00’
	unsigned int protection_absent;  //1 bslbf  表示是否误码校验
	unsigned int profile; //2 uimsbf  表示使用哪个级别的AAC，如01 Low Complexity(LC)--- AACLC
	unsigned int sf_index;           //4 uimsbf  表示使用的采样率下标
	unsigned int private_bit;        //1 bslbf
	unsigned int channel_configuration;  //3 uimsbf  表示声道数
	unsigned int original;               //1 bslbf
	unsigned int home;                   //1 bslbf
										 //下面的为改变的参数即每一帧都不同
	unsigned int copyright_identification_bit;   //1 bslbf
	unsigned int copyright_identification_start; //1 bslbf
	unsigned int aac_frame_length; // 13 bslbf  一个ADTS帧的长度包括ADTS头和raw data block
	unsigned int adts_buffer_fullness;           //11 bslbf     0x7FF 说明是码率可变的码流
//no_raw_data_blocks_in_frame 表示ADTS帧中有number_of_raw_data_blocks_in_frame + 1个AAC原始帧.
//所以说number_of_raw_data_blocks_in_frame == 0
//表示说ADTS帧中有一个AAC数据块并不是说没有。(一个AAC原始帧包含一段时间内1024个采样及相关数据)
	unsigned int no_raw_data_blocks_in_frame;    //2 uimsfb
	unsigned char data[2 * 1024 + 7];
	uint16_t sequence;
	uint32_t timeStamp;
} AdtsFrame;

void	makeAdtsHeader(const string &strAudioCfg,AdtsFrame &adts);
void 	writeAdtsHeader(const AdtsFrame &adts, uint8_t *pcAdts) ;
string 	makeAdtsConfig(const uint8_t *pcAdts);
void 	getAACInfo(const AdtsFrame &adts,int &iSampleRate,int &iChannel);
bool 	getAVCInfo(const string &strSps,int &iVideoWidth, int &iVideoHeight, float  &iVideoFps);

#endif /* SRC_PLAYER_PLAYER_H_ */
