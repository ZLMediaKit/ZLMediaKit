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

#include <cstring>
#include "Player.h"
#include "H264/SPSParser.h"
#include "Util/logger.h"

using namespace ZL::Util;

static unsigned const samplingFrequencyTable[16] = { 96000, 88200,
					64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025,
					8000, 7350, 0, 0, 0 };

void writeAdtsHeader(const AdtsFrame &hed, uint8_t *pcAdts) {
	pcAdts[0] = (hed.syncword >> 4 & 0xFF); //8bit
	pcAdts[1] = (hed.syncword << 4 & 0xF0); //4 bit
	pcAdts[1] |= (hed.id << 3 & 0x08); //1 bit
	pcAdts[1] |= (hed.layer << 1 & 0x06); //2bit
	pcAdts[1] |= (hed.protection_absent & 0x01); //1 bit

	pcAdts[2] = (hed.profile << 6 & 0xC0); // 2 bit
	pcAdts[2] |= (hed.sf_index << 2 & 0x3C); //4bit
	pcAdts[2] |= (hed.private_bit << 1 & 0x02); //1 bit
	pcAdts[2] |= (hed.channel_configuration >> 2 & 0x03); //1 bit

	pcAdts[3] = (hed.channel_configuration << 6 & 0xC0);  // 2 bit
	pcAdts[3] |= (hed.original << 5 & 0x20);				//1 bit
	pcAdts[3] |= (hed.home << 4 & 0x10);				//1 bit
	pcAdts[3] |= (hed.copyright_identification_bit << 3 & 0x08);			//1 bit
	pcAdts[3] |= (hed.copyright_identification_start << 2 & 0x04);		//1 bit
	pcAdts[3] |= (hed.aac_frame_length >> 11 & 0x03);				//2 bit

	pcAdts[4] = (hed.aac_frame_length >> 3 & 0xFF);				//8 bit

	pcAdts[5] = (hed.aac_frame_length << 5 & 0xE0);				//3 bit
	pcAdts[5] |= (hed.adts_buffer_fullness >> 6 & 0x1F);				//5 bit

	pcAdts[6] = (hed.adts_buffer_fullness << 2 & 0xFC);				//6 bit
	pcAdts[6] |= (hed.no_raw_data_blocks_in_frame & 0x03);				//2 bit
}
string 	makeAdtsConfig(const uint8_t *pcAdts){
	if (!(pcAdts[0] == 0xFF && (pcAdts[1] & 0xF0) == 0xF0)) {
		return "";
	}
	// Get and check the 'profile':
	unsigned char profile = (pcAdts[2] & 0xC0) >> 6; // 2 bits
	if (profile == 3) {
		return "";
	}

	// Get and check the 'sampling_frequency_index':
	unsigned char sampling_frequency_index = (pcAdts[2] & 0x3C) >> 2; // 4 bits
	if (samplingFrequencyTable[sampling_frequency_index] == 0) {
		return "";
	}

	// Get and check the 'channel_configuration':
	unsigned char channel_configuration = ((pcAdts[2] & 0x01) << 2)
			| ((pcAdts[3] & 0xC0) >> 6); // 3 bits

	unsigned char audioSpecificConfig[2];
	unsigned char const audioObjectType = profile + 1;
	audioSpecificConfig[0] = (audioObjectType << 3) | (sampling_frequency_index >> 1);
	audioSpecificConfig[1] = (sampling_frequency_index << 7) | (channel_configuration << 3);
	return string((char *)audioSpecificConfig,2);
}
void makeAdtsHeader(const string &strAudioCfg,AdtsFrame &adts) {
	uint8_t cfg1 = strAudioCfg[0];
	uint8_t cfg2 = strAudioCfg[1];

	int audioObjectType;
	int sampling_frequency_index;
	int channel_configuration;

	audioObjectType = cfg1 >> 3;
	sampling_frequency_index = ((cfg1 & 0x07) << 1) | (cfg2 >> 7);
	channel_configuration = (cfg2 & 0x7F) >> 3;

	adts.syncword = 0x0FFF;
	adts.id = 0;
	adts.layer = 0;
	adts.protection_absent = 1;
	adts.profile = audioObjectType - 1;
	adts.sf_index = sampling_frequency_index;
	adts.private_bit = 0;
	adts.channel_configuration = channel_configuration;
	adts.original = 0;
	adts.home = 0;
	adts.copyright_identification_bit = 0;
	adts.copyright_identification_start = 0;
	adts.aac_frame_length = 7;
	adts.adts_buffer_fullness = 2047;
	adts.no_raw_data_blocks_in_frame = 0;
}
void getAACInfo(const AdtsFrame &adts,int &iSampleRate,int &iChannel){
	iSampleRate = samplingFrequencyTable[adts.sf_index];
	iChannel = adts.channel_configuration;
}
bool getAVCInfo(const string& strSps,int &iVideoWidth, int &iVideoHeight, float  &iVideoFps) {
	T_GetBitContext tGetBitBuf;
	T_SPS tH264SpsInfo;
	memset(&tGetBitBuf,0,sizeof(tGetBitBuf));
	memset(&tH264SpsInfo,0,sizeof(tH264SpsInfo));
	tGetBitBuf.pu8Buf = (uint8_t *)strSps.data() + 1;
	tGetBitBuf.iBufSize = strSps.size() - 1;
	if(0 != h264DecSeqParameterSet((void *) &tGetBitBuf, &tH264SpsInfo)){
		return false;
	}
	h264GetWidthHeight(&tH264SpsInfo, &iVideoWidth, &iVideoHeight);
	h264GeFramerate(&tH264SpsInfo, &iVideoFps);
	//FatalL << iVideoWidth << " " << iVideoHeight << " " << iVideoFps;
	return true;
}
