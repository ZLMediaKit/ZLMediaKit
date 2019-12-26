/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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
#include "AACRtp.h"

namespace mediakit{

AACRtpEncoder::AACRtpEncoder(uint32_t ui32Ssrc,
                             uint32_t ui32MtuSize,
                             uint32_t ui32SampleRate,
                             uint8_t ui8PlayloadType,
                             uint8_t ui8Interleaved) :
        RtpInfo(ui32Ssrc,
                ui32MtuSize,
                ui32SampleRate,
                ui8PlayloadType,
                ui8Interleaved){
}

void AACRtpEncoder::inputFrame(const Frame::Ptr &frame) {
    GET_CONFIG(uint32_t, cycleMS, Rtp::kCycleMS);
    auto uiStamp = frame->stamp();
    auto pcData = frame->data() + frame->prefixSize();
    auto iLen = frame->size() - frame->prefixSize();

    uiStamp %= cycleMS;
    char *ptr = (char *) pcData;
    int iSize = iLen;
    while (iSize > 0) {
        if (iSize <= _ui32MtuSize - 20) {
            _aucSectionBuf[0] = 0;
            _aucSectionBuf[1] = 16;
            _aucSectionBuf[2] = iLen >> 5;
            _aucSectionBuf[3] = (iLen & 0x1F) << 3;
            memcpy(_aucSectionBuf + 4, ptr, iSize);
            makeAACRtp(_aucSectionBuf, iSize + 4, true, uiStamp);
            break;
        }
        _aucSectionBuf[0] = 0;
        _aucSectionBuf[1] = 16;
        _aucSectionBuf[2] = (iLen) >> 5;
        _aucSectionBuf[3] = (iLen & 0x1F) << 3;
        memcpy(_aucSectionBuf + 4, ptr, _ui32MtuSize - 20);
        makeAACRtp(_aucSectionBuf, _ui32MtuSize - 16, false, uiStamp);
        ptr += (_ui32MtuSize - 20);
        iSize -= (_ui32MtuSize - 20);
    }
}

void AACRtpEncoder::makeAACRtp(const void *data, unsigned int len, bool mark, uint32_t uiStamp) {
    RtpCodec::inputRtp(makeRtp(getTrackType(),data,len,mark,uiStamp), false);
}

/////////////////////////////////////////////////////////////////////////////////////

AACRtpDecoder::AACRtpDecoder(const Track::Ptr &track){
    auto aacTrack = dynamic_pointer_cast<AACTrack>(track);
    if(!aacTrack || !aacTrack->ready()){
        WarnL << "该aac track无效!";
    }else{
        _aac_cfg = aacTrack->getAacCfg();
    }
    _adts = obtainFrame();
}
AACRtpDecoder::AACRtpDecoder() {
    _adts = obtainFrame();
}

AACFrame::Ptr AACRtpDecoder::obtainFrame() {
    //从缓存池重新申请对象，防止覆盖已经写入环形缓存的对象
    auto frame = ResourcePoolHelper<AACFrame>::obtainObj();
    frame->aac_frame_length = 7;
    frame->iPrefixSize = 7;
    if(frame->syncword == 0 && !_aac_cfg.empty()) {
        makeAdtsHeader(_aac_cfg,*frame);
    }
    return frame;
}

bool AACRtpDecoder::inputRtp(const RtpPacket::Ptr &rtppack, bool key_pos) {
	// 获取rtp数据长度
    int length = rtppack->size() - rtppack->offset;

	// 获取rtp数据
	const uint8_t *rtp_packet_buf = (uint8_t *)rtppack->data() + rtppack->offset;
	
	do
	{
		// 查询头部的偏移，每次2字节
		uint32_t au_header_offset = 0;
		//首2字节表示Au-Header的长度，单位bit，所以除以16得到Au-Header字节数
		const uint16_t au_header_length = (((rtp_packet_buf[au_header_offset] << 8) | rtp_packet_buf[au_header_offset + 1]) >> 4);
		au_header_offset += 2;
		
		//assert(length > (2 + au_header_length * 2));
		if (length < (2 + au_header_length * 2))
			break;
		
		// 存放每一个aac帧长度
		std::vector<uint32_t > vec_aac_len;
		for (int i = 0; i < au_header_length; ++i)
		{
			// 之后的2字节是AU_HEADER
			const uint16_t au_header = ((rtp_packet_buf[au_header_offset] << 8) | rtp_packet_buf[au_header_offset + 1]);
			// 其中高13位表示一帧AAC负载的字节长度，低3位无用
			uint32_t nAac = (au_header >> 3);
			vec_aac_len.push_back(nAac);
			au_header_offset += 2;
		}

		// 真正aac负载开始处
		const uint8_t *rtp_packet_payload = rtp_packet_buf + au_header_offset;
		// 载荷查找
		uint32_t next_aac_payload_offset = 0;
		for (int j = 0; j < au_header_length; ++j)
		{
			// 当前aac包长度
			const uint32_t cur_aac_payload_len = vec_aac_len.at(j);

			if (_adts->aac_frame_length + cur_aac_payload_len > sizeof(AACFrame::buffer)) {
				_adts->aac_frame_length = 7;
				WarnL << "aac负载数据太长";
				return false;
			}
			
			// 提取每一包aac载荷数据
			memcpy(_adts->buffer + _adts->aac_frame_length, rtp_packet_payload + next_aac_payload_offset, cur_aac_payload_len);
			_adts->aac_frame_length += (cur_aac_payload_len);
			if (rtppack->mark == true) {
				_adts->sequence = rtppack->sequence;
				_adts->timeStamp = rtppack->timeStamp;
				writeAdtsHeader(*_adts, _adts->buffer);
				onGetAAC(_adts);
			}

			next_aac_payload_offset += cur_aac_payload_len;
		}
	} while (0);
	
    return false;
}

void AACRtpDecoder::onGetAAC(const AACFrame::Ptr &frame) {
    //写入环形缓存
    RtpCodec::inputFrame(frame);
    _adts = obtainFrame();
}


}//namespace mediakit



