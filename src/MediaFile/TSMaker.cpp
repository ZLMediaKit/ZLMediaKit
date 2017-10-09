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

#include "TSMaker.h"
#include "Util/logger.h"

using namespace ZL::Util;

TSMaker::TSMaker() {
	m_pOutVideoTs = NULL;
	m_pcFileBuf = NULL;
	m_uiWritePacketNum = 0;
	m_pVideo_pes = new TsPes();
	m_pAudio_pes = new TsPes();
	m_pVideo_pes->ESlen = 0;
	m_pAudio_pes->ESlen = 0;
	memset(&m_continuityCounter, 0, sizeof m_continuityCounter);
}

TSMaker::~TSMaker() {
	flush();
	if (m_pOutVideoTs != NULL) {
		fflush(m_pOutVideoTs);
		fclose(m_pOutVideoTs);
	}
	if (m_pcFileBuf != NULL) {
		delete[] m_pcFileBuf;
	}
	delete m_pVideo_pes;
	delete m_pAudio_pes;
}
void TSMaker::clear() {
	flush();
	if (m_pOutVideoTs != NULL) {
		fflush(m_pOutVideoTs);
		fclose(m_pOutVideoTs);
		m_pOutVideoTs = NULL;
	}
	m_uiWritePacketNum = 0;
	m_pVideo_pes->ESlen = 0;
	memset(&m_continuityCounter, 0, sizeof m_continuityCounter);

}
void TSMaker::flush() {
	unsigned char acTSbuf[TS_PACKET_SIZE];
	TsPacketHeader ts_header;
	if (m_pVideo_pes->ESlen == 0)
		return;
	unsigned char *pucTs = acTSbuf;
	if ((m_uiWritePacketNum % 40) == 0)             //每40个包打一个 pat,一个pmt
			{
		CreatePAT();                                         //创建PAT
		CreatePMT();                                         //创建PMT
	}
	memset(acTSbuf, 0, TS_PACKET_SIZE);
	CreateTsHeader(&ts_header, TS_H264_PID, 0x00, 0x03); //PID = TS_H264_PID,不是有效荷载单元起始指示符_play_init = 0x00, ada_field_C,0x03,含有调整字段和有效负载；
	TsHeader2buffer(&ts_header, acTSbuf);
	pucTs += 4;
	pucTs[0] = 184 - m_pVideo_pes->ESlen - 1;
	pucTs[1] = 0x00;
	pucTs += 2;
	memset(pucTs, 0xFF, (184 - m_pVideo_pes->ESlen - 2));
	pucTs += (184 - m_pVideo_pes->ESlen - 2);
	memcpy(pucTs, m_pVideo_pes->ES, m_pVideo_pes->ESlen);
	m_pVideo_pes->ESlen = 0;
	fwrite(acTSbuf, 188, 1, m_pOutVideoTs);   //将一包数据写入文件
	m_uiWritePacketNum++;
	return;
}

bool TSMaker::init(const string& filename, uint32_t bufsize) {
	m_strFilename = filename;
	if (m_pOutVideoTs == NULL) {
		m_pOutVideoTs = File::createfile_file(filename.c_str(), "wb");
		if (m_pOutVideoTs == NULL) {
			return false;
		}
	}

	if (m_pcFileBuf == NULL) {
		m_pcFileBuf = new char[bufsize];
		setvbuf(m_pOutVideoTs, m_pcFileBuf, _IOFBF, bufsize);
	}

	return true;

}

void TSMaker::CreatePAT() {
	TsPacketHeader ts_header;
	TsPat ts_pat;
	unsigned char aucPat[TS_PACKET_SIZE];
	unsigned char * pucPat;
	uint32_t ui32PAT_CRC = 0xFFFFFFFF;

	memset(aucPat, 0xFF, TS_PACKET_SIZE);
	pucPat = aucPat;
	CreateTsHeader(&ts_header, TS_PAT_PID, 0x01, 0x01); //PID = 0x00,有效荷载单元起始指示符_play_init = 0x01, ada_field_C,0x01,仅有有效负载 ；
	TsHeader2buffer(&ts_header, aucPat);
	pucPat[4] = 0;                                //自适应段的长度为0
	pucPat += 5;
	ts_pat.table_id = 0x00;
	ts_pat.section_syntax_indicator = 0x01;
	ts_pat.zero = 0x00;
	ts_pat.reserved_1 = 0x03;                         //设置为11；
	ts_pat.section_length = 0x0d;                     //pat结构体长度 16个字节减去上面的3个字节
	ts_pat.transport_stream_id = 0x01;
	ts_pat.reserved_2 = 0x03;                         //设置为11；
	ts_pat.version_number = 0x00;
	ts_pat.current_next_indicator = 0x01;             //当前的pat 有效
	ts_pat.section_number = 0x00;
	ts_pat.last_section_number = 0x00;
	ts_pat.program_number = 0x01;
	ts_pat.reserved_3 = 0x07;                         //设置为111；
	ts_pat.program_map_PID = TS_PMT_PID;              //PMT的PID
	ts_pat.CRC_32 = ui32PAT_CRC;                          //传输过程中检测的一种算法值 先设定一个填充值

	pucPat[0] = ts_pat.table_id;
	pucPat[1] = ts_pat.section_syntax_indicator << 7 | ts_pat.zero << 6
			| ts_pat.reserved_1 << 4 | ((ts_pat.section_length >> 8) & 0x0F);
	pucPat[2] = ts_pat.section_length & 0x00FF;
	pucPat[3] = ts_pat.transport_stream_id >> 8;
	pucPat[4] = ts_pat.transport_stream_id & 0x00FF;
	pucPat[5] = ts_pat.reserved_2 << 6 | ts_pat.version_number << 1
			| ts_pat.current_next_indicator;
	pucPat[6] = ts_pat.section_number;
	pucPat[7] = ts_pat.last_section_number;
	pucPat[8] = ts_pat.program_number >> 8;
	pucPat[9] = ts_pat.program_number & 0x00FF;
	pucPat[10] = ts_pat.reserved_3 << 5
			| ((ts_pat.program_map_PID >> 8) & 0x0F);
	pucPat[11] = ts_pat.program_map_PID & 0x00FF;
	pucPat += 12;
	ui32PAT_CRC = Zwg_ntohl(calc_crc32(aucPat + 5, pucPat - aucPat - 5));
	memcpy(pucPat, (unsigned char *) &ui32PAT_CRC, 4);
	fwrite(aucPat, 188, 1, m_pOutVideoTs); //将PAT包写入文件
	return;
}

void TSMaker::CreatePMT() {
	TsPacketHeader ts_header;
	TsPmt ts_pmt;
	unsigned char aucPmt[TS_PACKET_SIZE];
	unsigned char * pucPmt;
	uint32_t ui32PMT_CRC = 0xFFFFFFFF;
	int iLen = 0;

	memset(aucPmt, 0xFF, TS_PACKET_SIZE);                           //将一个包填成0xFF
	pucPmt = aucPmt;

	CreateTsHeader(&ts_header, TS_PMT_PID, 0x01, 0x01); //PID = 0x00,有效荷载单元起始指示符_play_init = 0x01, ada_field_C,0x01,仅有有效负载；
	TsHeader2buffer(&ts_header, aucPmt);
	pucPmt[4] = 0;                                              //自适应段的长度为0
	pucPmt += 5;
	ts_pmt.table_id = 0x02;
	ts_pmt.section_syntax_indicator = 0x01;
	ts_pmt.zero = 0x00;
	ts_pmt.reserved_1 = 0x03;
	ts_pmt.section_length = 0x17;             //PMT结构体长度 16 + 5 + 5个字节减去上面的3个字节
	ts_pmt.program_number = 01;                                        //只有一个节目
	ts_pmt.reserved_2 = 0x03;
	ts_pmt.version_number = 0x00;
	ts_pmt.current_next_indicator = 0x01;                            //当前的PMT有效
	ts_pmt.section_number = 0x00;
	ts_pmt.last_section_number = 0x00;
	ts_pmt.reserved_3 = 0x07;
	ts_pmt.PCR_PID = TS_H264_PID;                                       //视频PID
	ts_pmt.reserved_4 = 0x0F;
	ts_pmt.program_info_length = 0x00;                             //后面无 节目信息描述
	ts_pmt.stream_type_video = PMT_STREAM_TYPE_VIDEO;                   //视频的类型
	ts_pmt.reserved_5_video = 0x07;
	ts_pmt.elementary_PID_video = TS_H264_PID;                         //视频的PID
	ts_pmt.reserved_6_video = 0x0F;
	ts_pmt.ES_info_length_video = 0x00;                            //视频无跟随的相关信息
	ts_pmt.stream_type_audio = PMT_STREAM_TYPE_AUDIO;                    //音频类型
	ts_pmt.reserved_5_audio = 0x07;
	ts_pmt.elementary_PID_audio = TS_AAC_PID;                           //音频PID
	ts_pmt.reserved_6_audio = 0x0F;
	ts_pmt.ES_info_length_audio = 0x00;                            //音频无跟随的相关信息

	ts_pmt.CRC_32 = ui32PMT_CRC;

	pucPmt[0] = ts_pmt.table_id;
	pucPmt[1] = ts_pmt.section_syntax_indicator << 7 | ts_pmt.zero << 6
			| ts_pmt.reserved_1 << 4 | ((ts_pmt.section_length >> 8) & 0x0F);
	pucPmt[2] = ts_pmt.section_length & 0x00FF;
	pucPmt[3] = ts_pmt.program_number >> 8;
	pucPmt[4] = ts_pmt.program_number & 0x00FF;
	pucPmt[5] = ts_pmt.reserved_2 << 6 | ts_pmt.version_number << 1
			| ts_pmt.current_next_indicator;
	pucPmt[6] = ts_pmt.section_number;
	pucPmt[7] = ts_pmt.last_section_number;
	pucPmt[8] = ts_pmt.reserved_3 << 5 | ((ts_pmt.PCR_PID >> 8) & 0x1F);
	pucPmt[9] = ts_pmt.PCR_PID & 0x0FF;
	pucPmt[10] = ts_pmt.reserved_4 << 4
			| ((ts_pmt.program_info_length >> 8) & 0x0F);
	pucPmt[11] = ts_pmt.program_info_length & 0xFF;
	pucPmt[12] = ts_pmt.stream_type_video;               //视频流的stream_type
	pucPmt[13] = ts_pmt.reserved_5_video << 5
			| ((ts_pmt.elementary_PID_video >> 8) & 0x1F);
	pucPmt[14] = ts_pmt.elementary_PID_video & 0x00FF;
	pucPmt[15] = ts_pmt.reserved_6_video << 4
			| ((ts_pmt.ES_info_length_video >> 8) & 0x0F);
	pucPmt[16] = ts_pmt.ES_info_length_video & 0x0FF;
	pucPmt[17] = ts_pmt.stream_type_audio;               //音频流的stream_type
	pucPmt[18] = ts_pmt.reserved_5_audio << 5
			| ((ts_pmt.elementary_PID_audio >> 8) & 0x1F);
	pucPmt[19] = ts_pmt.elementary_PID_audio & 0x00FF;
	pucPmt[20] = ts_pmt.reserved_6_audio << 4
			| ((ts_pmt.ES_info_length_audio >> 8) & 0x0F);
	pucPmt[21] = ts_pmt.ES_info_length_audio & 0x0FF;
	pucPmt += 22;

	iLen = pucPmt - aucPmt - 8 + 4;
	iLen = iLen > 0xffff ? 0 : iLen;
	*(aucPmt + 6) = 0xb0 | (iLen >> 8);
	*(aucPmt + 7) = iLen;

	ui32PMT_CRC = Zwg_ntohl(calc_crc32(aucPmt + 5, pucPmt - aucPmt - 5));
	memcpy(pucPmt, (unsigned char *) &ui32PMT_CRC, 4);
	fwrite(aucPmt, 188, 1, m_pOutVideoTs);                             //将PAT包写入文件
}

void TSMaker::CreateTsHeader(TsPacketHeader* pTsHeader, unsigned int uiPID, unsigned char ucPlayInit, unsigned char ucAdaFieldC) {
	pTsHeader->sync_byte = TS_SYNC_BYTE;
	pTsHeader->tras_error = 0x00;
	pTsHeader->play_init = ucPlayInit;
	pTsHeader->tras_prio = 0x00;
	pTsHeader->PID = uiPID;
	pTsHeader->tras_scramb = 0x00;
	pTsHeader->ada_field_C = ucAdaFieldC;

	if (uiPID == TS_PAT_PID) {                             //这是pat的包
		pTsHeader->conti_cter = (m_continuityCounter.continuity_counter_pat % 16);
		m_continuityCounter.continuity_counter_pat++;
	} else if (uiPID == TS_PMT_PID) {                             //这是pmt的包
		pTsHeader->conti_cter = (m_continuityCounter.continuity_counter_pmt % 16);
		m_continuityCounter.continuity_counter_pmt++;
	} else if (uiPID == TS_H264_PID) {                             //这是H264的包
		pTsHeader->conti_cter = (m_continuityCounter.continuity_counter_video % 16);
		m_continuityCounter.continuity_counter_video++;
	} else if (uiPID == TS_AAC_PID) {                             //这是MP3的包
		pTsHeader->conti_cter = (m_continuityCounter.continuity_counter_audio % 16);
		m_continuityCounter.continuity_counter_audio++;
	} else {                             //其他包出错，或可扩展
		WarnL << "continuity_counter error packet";
	}
}

void TSMaker::TsHeader2buffer(TsPacketHeader* pTsHeader, unsigned char* pucBuffer) {
	pucBuffer[0] = pTsHeader->sync_byte;
	pucBuffer[1] = 	pTsHeader->tras_error << 7 | pTsHeader->play_init << 6 |
					pTsHeader->tras_prio << 5 | ((pTsHeader->PID >> 8) & 0x1f);
	pucBuffer[2] = (pTsHeader->PID & 0x00ff);
	pucBuffer[3] = pTsHeader->tras_scramb << 6 | pTsHeader->ada_field_C << 4 | pTsHeader->conti_cter;

}

void TSMaker::WriteAdaptive_flags_Head( Ts_Adaptation_field * pTsAdaptationField, uint64_t ui64VideoPts) {
	//填写自适应段
	pTsAdaptationField->discontinuty_indicator = 0;
	pTsAdaptationField->random_access_indicator = 0;
	pTsAdaptationField->elementary_stream_priority_indicator = 0;
	pTsAdaptationField->PCR_flag = 1;                                   //只用到这个
	pTsAdaptationField->OPCR_flag = 0;
	pTsAdaptationField->splicing_point_flag = 0;
	pTsAdaptationField->transport_private_data_flag = 0;
	pTsAdaptationField->adaptation_field_extension_flag = 0;

	//需要自己算
	pTsAdaptationField->pcr = ui64VideoPts * 300;
	pTsAdaptationField->adaptation_field_length = 7;                     //占用7位

	pTsAdaptationField->opcr = 0;
	pTsAdaptationField->splice_countdown = 0;
	pTsAdaptationField->private_data_len = 0;
}

int TSMaker::inputH264(const char* pcData, uint32_t ui32Len, uint64_t ui64Time) {
	if (m_pOutVideoTs == NULL) {
		return false;
	}
	m_pVideo_pes->ES = const_cast<char *>(pcData);
	m_pVideo_pes->ESlen = ui32Len;
	Ts_Adaptation_field ts_adaptation_field_Head;
	WriteAdaptive_flags_Head(&ts_adaptation_field_Head, ui64Time); //填写自适应段标志帧头
	m_pVideo_pes->packet_start_code_prefix = 0x000001;
	m_pVideo_pes->stream_id = TS_H264_STREAM_ID; //E0~EF表示是视频的,C0~DF是音频,H264-- E0
	m_pVideo_pes->marker_bit = 0x02;
	m_pVideo_pes->PES_scrambling_control = 0x00;                   //人选字段 存在，不加扰
	m_pVideo_pes->PES_priority = 0x00;
	m_pVideo_pes->data_alignment_indicator = 0x00;
	m_pVideo_pes->copyright = 0x00;
	m_pVideo_pes->original_or_copy = 0x00;
	m_pVideo_pes->PTS_DTS_flags = 0x03;
	m_pVideo_pes->ESCR_flag = 0x00;
	m_pVideo_pes->ES_rate_flag = 0x00;
	m_pVideo_pes->DSM_trick_mode_flag = 0x00;
	m_pVideo_pes->additional_copy_info_flag = 0x00;
	m_pVideo_pes->PES_CRC_flag = 0x00;
	m_pVideo_pes->PES_extension_flag = 0x00;
	m_pVideo_pes->PES_header_data_length = 0x0A; //后面的数据包括了PTS和 DTS所占的字节数
	PES2TS(m_pVideo_pes, TS_H264_PID, &ts_adaptation_field_Head, ui64Time);
	m_pVideo_pes->ESlen = 0;
	return ui32Len;
}

int TSMaker::inputAAC(const char* pcData, uint32_t ui32Len, uint64_t ui64Pts) {
	if (m_pOutVideoTs == NULL) {
		return 0;
	}
	m_pAudio_pes->ES = const_cast<char *>(pcData);
	m_pAudio_pes->ESlen = ui32Len;
	Ts_Adaptation_field ts_adaptation_field_Head;
	WriteAdaptive_flags_Tail(&ts_adaptation_field_Head); //填写自适应段标志帧头
	m_pAudio_pes->packet_start_code_prefix = 0x000001;
	m_pAudio_pes->stream_id = TS_AAC_STREAM_ID; //E0~EF表示是视频的,C0~DF是音频,H264-- E0
	m_pAudio_pes->marker_bit = 0x02;
	m_pAudio_pes->PES_scrambling_control = 0x00;                   //人选字段 存在，不加扰
	m_pAudio_pes->PES_priority = 0x00;
	m_pAudio_pes->data_alignment_indicator = 0x00;
	m_pAudio_pes->copyright = 0x00;
	m_pAudio_pes->original_or_copy = 0x00;
	m_pAudio_pes->PTS_DTS_flags = 0x03;
	m_pAudio_pes->ESCR_flag = 0x00;
	m_pAudio_pes->ES_rate_flag = 0x00;
	m_pAudio_pes->DSM_trick_mode_flag = 0x00;
	m_pAudio_pes->additional_copy_info_flag = 0x00;
	m_pAudio_pes->PES_CRC_flag = 0x00;
	m_pAudio_pes->PES_extension_flag = 0x00;
	m_pAudio_pes->PES_header_data_length = 0x0A; //后面的数据包括了PTS
	PES2TS(m_pAudio_pes, TS_AAC_PID, &ts_adaptation_field_Head, ui64Pts);
	m_pAudio_pes->ESlen = 0;
	return ui32Len;
}

void TSMaker::WriteAdaptive_flags_Tail(Ts_Adaptation_field* pTsAdaptationField) {
	//填写自适应段
	pTsAdaptationField->discontinuty_indicator = 0;
	pTsAdaptationField->random_access_indicator = 0;
	pTsAdaptationField->elementary_stream_priority_indicator = 0;
	pTsAdaptationField->PCR_flag = 0;                                   //只用到这个
	pTsAdaptationField->OPCR_flag = 0;
	pTsAdaptationField->splicing_point_flag = 0;
	pTsAdaptationField->transport_private_data_flag = 0;
	pTsAdaptationField->adaptation_field_extension_flag = 0;

	//需要自己算
	pTsAdaptationField->pcr = 0;
	pTsAdaptationField->adaptation_field_length = 1;               //占用1位标志所用的位

	pTsAdaptationField->opcr = 0;
	pTsAdaptationField->splice_countdown = 0;
	pTsAdaptationField->private_data_len = 0;
}

void TSMaker::CreateAdaptive_Ts(Ts_Adaptation_field * pTsAdaptationField, unsigned char * pucTs, unsigned int uiAdaptiveLength) {
	unsigned int uiCurrentAdaptiveLength = 1;                       //当前已经用的自适应段长度
	unsigned char ucAdaptiveflags = 0;                                   //自适应段的标志
	unsigned char *pucTmp = pucTs;
	//填写自适应字段
	if (pTsAdaptationField->adaptation_field_length > 0) {
		pucTs += 1;                                     //自适应段的一些标志所占用的1个字节
		uiCurrentAdaptiveLength += 1;

		if (pTsAdaptationField->discontinuty_indicator) {
			ucAdaptiveflags |= 0x80;
		}
		if (pTsAdaptationField->random_access_indicator) {
			ucAdaptiveflags |= 0x40;
		}
		if (pTsAdaptationField->elementary_stream_priority_indicator) {
			ucAdaptiveflags |= 0x20;
		}
		if (pTsAdaptationField->PCR_flag) {
			unsigned long long pcr_base;
			unsigned int pcr_ext;

			pcr_base = (pTsAdaptationField->pcr / 300);
			pcr_ext = (pTsAdaptationField->pcr % 300);

			ucAdaptiveflags |= 0x10;

			pucTs[0] = (pcr_base >> 25) & 0xff;
			pucTs[1] = (pcr_base >> 17) & 0xff;
			pucTs[2] = (pcr_base >> 9) & 0xff;
			pucTs[3] = (pcr_base >> 1) & 0xff;
			pucTs[4] = pcr_base << 7 | pcr_ext >> 8 | 0x7e;
			pucTs[5] = (pcr_ext) & 0xff;
			pucTs += 6;

			uiCurrentAdaptiveLength += 6;
		}
		if (pTsAdaptationField->OPCR_flag) {
			unsigned long long opcr_base;
			unsigned int opcr_ext;

			opcr_base = (pTsAdaptationField->opcr / 300);
			opcr_ext = (pTsAdaptationField->opcr % 300);

			ucAdaptiveflags |= 0x08;

			pucTs[0] = (opcr_base >> 25) & 0xff;
			pucTs[1] = (opcr_base >> 17) & 0xff;
			pucTs[2] = (opcr_base >> 9) & 0xff;
			pucTs[3] = (opcr_base >> 1) & 0xff;
			pucTs[4] = ((opcr_base << 7) & 0x80)
					| ((opcr_ext >> 8) & 0x01);
			pucTs[5] = (opcr_ext) & 0xff;
			pucTs += 6;
			uiCurrentAdaptiveLength += 6;
		}
		if (pTsAdaptationField->splicing_point_flag) {
			pucTs[0] = pTsAdaptationField->splice_countdown;

			ucAdaptiveflags |= 0x04;

			pucTs += 1;
			uiCurrentAdaptiveLength += 1;
		}
		if (pTsAdaptationField->private_data_len > 0) {
			ucAdaptiveflags |= 0x02;
			if (1 + pTsAdaptationField->private_data_len
					> static_cast<unsigned char>(uiAdaptiveLength
							- uiCurrentAdaptiveLength)) {
				WarnL << "private_data_len error !";
				return;
			} else {
				pucTs[0] = pTsAdaptationField->private_data_len;
				pucTs += 1;
				memcpy(pucTs, pTsAdaptationField->private_data,
						pTsAdaptationField->private_data_len);
				pucTs += pTsAdaptationField->private_data_len;

				uiCurrentAdaptiveLength += (1
						+ pTsAdaptationField->private_data_len);
			}
		}
		if (pTsAdaptationField->adaptation_field_extension_flag) {
			ucAdaptiveflags |= 0x01;
			pucTs[1] = 1;
			pucTs[2] = 0;
			uiCurrentAdaptiveLength += 2;
		}
		*pucTmp = ucAdaptiveflags;    //将标志放入内存
	}
	return;
}
void TSMaker::PES2TS(TsPes * pTsPes, unsigned int uiPID, Ts_Adaptation_field * pTsAdaptationFieldHead, uint64_t ui64Dts) {
	TsPacketHeader ts_header;
	unsigned int uiAdaptiveLength = 0;                                //要填写0XFF的长度
	unsigned int uiFirstPacketLoadLength = 188 - 4 - 1 - pTsAdaptationFieldHead->adaptation_field_length - 19; //分片包的第一个包的负载长度
	const char * pcNeafBuf = pTsPes->ES;                              //分片包 总负载的指针
	unsigned char aucTSbuf[TS_PACKET_SIZE];
	unsigned char * pucTSBuf;
	bool bFirstPkt = true;
	while (true) {
		if ((m_uiWritePacketNum++ % 40) == 0)                   //每40个包打一个 pat,一个pmt
				{
			CreatePAT();                                                 //创建PAT
			CreatePMT();                                                 //创建PMT
		}
		if (bFirstPkt) {
			bFirstPkt = false;
			//memset(TSbuf,0,TS_PACKET_SIZE);
			pucTSBuf = aucTSbuf;
			CreateTsHeader(&ts_header, uiPID, 0x01, 0x03); //PID = TS_H264_PID,有效荷载单元起始指示符_play_init = 0x01, ada_field_C,0x03,含有调整字段和有效负载 ；
			TsHeader2buffer(&ts_header, aucTSbuf);
			pucTSBuf += 4; //写入TS 头
			if (pTsPes->ESlen > uiFirstPacketLoadLength) {
				//计算分片包的第一个包的负载长度
				uiAdaptiveLength = 188 - 4 - 1 - ((pTsPes->ESlen - uiFirstPacketLoadLength) % 184); //要填写0XFF的长度,最后一个包有自适应
				pucTSBuf[0] = pTsAdaptationFieldHead->adaptation_field_length; //自适应字段的长度，自己填写的
				pucTSBuf += 1;
				CreateAdaptive_Ts(pTsAdaptationFieldHead, pucTSBuf, (uiAdaptiveLength)); //填写自适应字段
				pucTSBuf += pTsAdaptationFieldHead->adaptation_field_length; //填写自适应段所需要的长度
			} else {
				uiAdaptiveLength = uiFirstPacketLoadLength - pTsPes->ESlen;
				pucTSBuf[0] = pTsAdaptationFieldHead->adaptation_field_length + uiAdaptiveLength; //自适应字段的长度，自己填写的
				pucTSBuf += 1;
				CreateAdaptive_Ts(pTsAdaptationFieldHead, pucTSBuf, uiAdaptiveLength); //填写自适应字段
				pucTSBuf += pTsAdaptationFieldHead->adaptation_field_length;
				memset(pucTSBuf, 0xFF, uiAdaptiveLength);
				pucTSBuf += uiAdaptiveLength;
				uiFirstPacketLoadLength = pTsPes->ESlen;
			}

			pTsPes->PES_packet_length = pTsPes->ESlen + pTsPes->PES_header_data_length + 3;
			if (TS_H264_PID==uiPID || pTsPes->PES_packet_length > 0xFFFF) {
				pTsPes->PES_packet_length = 0;
			}
			pucTSBuf[0] = (pTsPes->packet_start_code_prefix >> 16) & 0xFF;
			pucTSBuf[1] = (pTsPes->packet_start_code_prefix >> 8) & 0xFF;
			pucTSBuf[2] = pTsPes->packet_start_code_prefix & 0xFF;
			pucTSBuf[3] = pTsPes->stream_id;
			pucTSBuf[4] = (pTsPes->PES_packet_length >> 8) & 0xFF;
			pucTSBuf[5] = pTsPes->PES_packet_length & 0xFF;
			pucTSBuf[6] = pTsPes->marker_bit << 6
					| pTsPes->PES_scrambling_control << 4
					| pTsPes->PES_priority << 3
					| pTsPes->data_alignment_indicator << 2
					| pTsPes->copyright << 1 | pTsPes->original_or_copy;
			pucTSBuf[7] = pTsPes->PTS_DTS_flags << 6 | pTsPes->ESCR_flag << 5
					| pTsPes->ES_rate_flag << 4
					| pTsPes->DSM_trick_mode_flag << 3
					| pTsPes->additional_copy_info_flag << 2
					| pTsPes->PES_CRC_flag << 1 | pTsPes->PES_extension_flag;
			pucTSBuf += 8;
			switch (pTsPes->PTS_DTS_flags) {
			case 0x03: //both pts and ui64Dts
				pucTSBuf[6] = (((0x1 << 4) | ((ui64Dts >> 29) & 0x0E) | 0x01) & 0xff);
				pucTSBuf[7] = (((((ui64Dts >> 14) & 0xfffe) | 0x01) >> 8) & 0xff);
				pucTSBuf[8] = ((((ui64Dts >> 14) & 0xfffe) | 0x01) & 0xff);
				pucTSBuf[9] = ((((ui64Dts << 1) & 0xfffe) | 0x01) >> 8) & 0xff;
				pucTSBuf[10] = (((ui64Dts << 1) & 0xfffe) | 0x01) & 0xff;
			case 0x02: //pts only
				pucTSBuf[1] = (((0x3 << 4) | ((ui64Dts >> 29) & 0x0E) | 0x01) & 0xff);
				pucTSBuf[2] = (((((ui64Dts >> 14) & 0xfffe) | 0x01) >> 8) & 0xff);
				pucTSBuf[3] = ((((ui64Dts >> 14) & 0xfffe) | 0x01) & 0xff);
				pucTSBuf[4] = (((((ui64Dts << 1) & 0xfffe) | 0x01) >> 8) & 0xff);
				pucTSBuf[5] = ((((ui64Dts << 1) & 0xfffe) | 0x01) & 0xff);
				break;
			default:
				break;
			}
			pucTSBuf[0] = pTsPes->PES_header_data_length;
			pucTSBuf += (1 + pucTSBuf[0]);
			memcpy(pucTSBuf, pcNeafBuf, uiFirstPacketLoadLength);
			pcNeafBuf += uiFirstPacketLoadLength;
			pTsPes->ESlen -= uiFirstPacketLoadLength;
			//将包写入文件
			fwrite(aucTSbuf, 188, 1, m_pOutVideoTs);                      //将一包数据写入文件
			continue;
		}
		if (pTsPes->ESlen >= 184) {
			//处理中间包
			//memset(TSbuf,0,TS_PACKET_SIZE);
			pucTSBuf = aucTSbuf;
			CreateTsHeader(&ts_header, uiPID, 0x00, 0x01); //PID = TS_H264_PID,不是有效荷载单元起始指示符_play_init = 0x00, ada_field_C,0x01,仅有有效负载；
			TsHeader2buffer(&ts_header, aucTSbuf);
			pucTSBuf += 4;
			memcpy(pucTSBuf, pcNeafBuf, 184);
			pcNeafBuf += 184;
			pTsPes->ESlen -= 184;
			fwrite(aucTSbuf, 188, 1, m_pOutVideoTs);
			continue;
		}
		if (pTsPes->ESlen == 183) {
			//memset(TSbuf,0,TS_PACKET_SIZE);
			pucTSBuf = aucTSbuf;
			CreateTsHeader(&ts_header, uiPID, 0x00, 0x03); //PID = TS_H264_PID,不是有效荷载单元起始指示符_play_init = 0x00, ada_field_C,0x03,含有调整字段和有效负载；
			TsHeader2buffer(&ts_header, aucTSbuf);
			pucTSBuf += 4;
			pucTSBuf[0] = 1;
			pucTSBuf[1] = 0x00;
			pucTSBuf += 2;
			memcpy(pucTSBuf, pcNeafBuf, 182);
			pTsPes->ESlen = 1;
			fwrite(aucTSbuf, 188, 1, m_pOutVideoTs);   //将一包数据写入文件

		}
		//memset(TSbuf,0,TS_PACKET_SIZE);
		pucTSBuf = aucTSbuf;
		CreateTsHeader(&ts_header, uiPID, 0x00, 0x03); //PID = TS_H264_PID,不是有效荷载单元起始指示符_play_init = 0x00, ada_field_C,0x03,含有调整字段和有效负载；
		TsHeader2buffer(&ts_header, aucTSbuf);
		pucTSBuf += 4;
		pucTSBuf[0] = 184 - pTsPes->ESlen - 1;
		pucTSBuf[1] = 0x00;
		pucTSBuf += 2;

		memset(pucTSBuf, 0xFF, (184 - pTsPes->ESlen - 2));
		pucTSBuf += (184 - pTsPes->ESlen - 2);

		memcpy(pucTSBuf, pcNeafBuf, pTsPes->ESlen); //183就丢弃一字节
		pTsPes->ESlen = 0;
		fwrite(aucTSbuf, 188, 1, m_pOutVideoTs);   //将一包数据写入文件
		break;
	}
}

