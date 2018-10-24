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

#include <string.h>
#include "Common/config.h"
#include "Rtmp/Rtmp.h"
#include "RtspToRtmpMediaSource.h"
#include "Util/util.h"
#include "Network/sockutil.h"
using namespace toolkit;

namespace mediakit {

RtspToRtmpMediaSource::RtspToRtmpMediaSource(const string &vhost,
                                             const string &app,
                                             const string &id,
                                             bool bEnableHls,
                                             bool bEnableMp4) :
		RtspMediaSource(vhost,app,id),_bEnableHls(bEnableHls),_bEnableMp4(bEnableMp4) {
}

RtspToRtmpMediaSource::~RtspToRtmpMediaSource() {

}

void RtspToRtmpMediaSource::makeVideoConfigPkt() {
	int8_t flags = 7; //h.264
	flags |= (FLV_KEY_FRAME << 4);
	bool is_config = true;

	RtmpPacket::Ptr rtmpPkt(new RtmpPacket);
//////////header
	rtmpPkt->strBuf.push_back(flags);
	rtmpPkt->strBuf.push_back(!is_config);
	rtmpPkt->strBuf.append("\x0\x0\x0", 3);

////////////sps
	rtmpPkt->strBuf.push_back(1); // version

	//todo(xzl) 修复此处
	string _sps ;//= _pParser->getSps().substr(4);
	string _pps ;//= _pParser->getPps().substr(4);
	//DebugL<<hexdump(_sps.data(), _sps.size());
	rtmpPkt->strBuf.push_back(_sps[1]); // profile
	rtmpPkt->strBuf.push_back(_sps[2]); // compat
	rtmpPkt->strBuf.push_back(_sps[3]); // level
	rtmpPkt->strBuf.push_back(0xff); // 6 bits reserved + 2 bits nal size length - 1 (11)
	rtmpPkt->strBuf.push_back(0xe1); // 3 bits reserved + 5 bits number of sps (00001)
	uint16_t size = _sps.size();
	size = htons(size);
	rtmpPkt->strBuf.append((char *) &size, 2);
	rtmpPkt->strBuf.append(_sps);

/////////////pps
	rtmpPkt->strBuf.push_back(1); // version
	size = _pps.size();
	size = htons(size);
	rtmpPkt->strBuf.append((char *) &size, 2);
	rtmpPkt->strBuf.append(_pps);

	rtmpPkt->bodySize = rtmpPkt->strBuf.size();
	rtmpPkt->chunkId = CHUNK_VIDEO;
	rtmpPkt->streamId = STREAM_MEDIA;
	rtmpPkt->timeStamp = 0;
	rtmpPkt->typeId = MSG_VIDEO;
	_pRtmpSrc->onGetMedia(rtmpPkt);
}
void RtspToRtmpMediaSource::onGetH264(const H264Frame& frame) {
	if(_pRecorder){
		_pRecorder->inputH264((char *) frame.data(), frame.size(), frame.timeStamp, frame.type);
	}
	uint8_t nal_type = frame.data()[4] & 0x1F;
	int8_t flags = 7; //h.264
	bool is_config = false;
	switch (nal_type) {
	case 7:
	case 8:
		return;
	case 5:
		flags |= (FLV_KEY_FRAME << 4);
		break;
	default:
		flags |= (FLV_INTER_FRAME << 4);
		break;
	}
	RtmpPacket::Ptr rtmpPkt(new RtmpPacket);
	rtmpPkt->strBuf.push_back(flags);
	rtmpPkt->strBuf.push_back(!is_config);
	rtmpPkt->strBuf.append("\x0\x0\x0", 3);
	uint32_t size = frame.size() - 4;
	size = htonl(size);
	rtmpPkt->strBuf.append((char *) &size, 4);
	rtmpPkt->strBuf.append(&frame.data()[4], frame.size() - 4);

	rtmpPkt->bodySize = rtmpPkt->strBuf.size();
	rtmpPkt->chunkId = CHUNK_VIDEO;
	rtmpPkt->streamId = STREAM_MEDIA;
	rtmpPkt->timeStamp = frame.timeStamp;
	rtmpPkt->typeId = MSG_VIDEO;
	_pRtmpSrc->onGetMedia(rtmpPkt);
}
void RtspToRtmpMediaSource::onGetAAC(const AACFrame& frame) {
	if(_pRecorder){
		_pRecorder->inputAAC((char *) frame.buffer, frame.aac_frame_length, frame.timeStamp);
	}

	RtmpPacket::Ptr rtmpPkt(new RtmpPacket);
//////////header
	uint8_t is_config = false;
	rtmpPkt->strBuf.push_back(_ui8AudioFlags);
	rtmpPkt->strBuf.push_back(!is_config);
	rtmpPkt->strBuf.append((char *) frame.buffer + 7, frame.aac_frame_length - 7);

	rtmpPkt->bodySize = rtmpPkt->strBuf.size();
	rtmpPkt->chunkId = CHUNK_AUDIO;
	rtmpPkt->streamId = STREAM_MEDIA;
	rtmpPkt->timeStamp = frame.timeStamp;
	rtmpPkt->typeId = MSG_AUDIO;
	_pRtmpSrc->onGetMedia(rtmpPkt);
}

void RtspToRtmpMediaSource::makeAudioConfigPkt() {
	//todo(xzl) 修复此处
#if 0
	uint8_t flvStereoOrMono = (_pParser->getAudioChannel() > 1);
	uint8_t flvSampleRate;
	switch (_pParser->getAudioSampleRate()) {
	case 48000:
	case 44100:
		flvSampleRate = 3;
		break;
	case 24000:
	case 22050:
		flvSampleRate = 2;
		break;
	case 12000:
	case 11025:
		flvSampleRate = 1;
		break;
	default:
		flvSampleRate = 0;
		break;
	}
	uint8_t flvSampleBit = _pParser->getAudioSampleBit() == 16;
	uint8_t flvAudioType = 10; //aac

	_ui8AudioFlags = (flvAudioType << 4) | (flvSampleRate << 2) | (flvSampleBit << 1) | flvStereoOrMono;

	RtmpPacket::Ptr rtmpPkt(new RtmpPacket);
//////////header
	uint8_t is_config = true;
	rtmpPkt->strBuf.push_back(_ui8AudioFlags);
	rtmpPkt->strBuf.push_back(!is_config);
	rtmpPkt->strBuf.append(_pParser->getAudioCfg());

	rtmpPkt->bodySize = rtmpPkt->strBuf.size();
	rtmpPkt->chunkId = CHUNK_AUDIO;
	rtmpPkt->streamId = STREAM_MEDIA;
	rtmpPkt->timeStamp = 0;
	rtmpPkt->typeId = MSG_AUDIO;
	_pRtmpSrc->onGetMedia(rtmpPkt);

#endif
}

void RtspToRtmpMediaSource::makeMetaData() {
	_pRtmpSrc.reset(new RtmpMediaSource(getVhost(),getApp(),getId()));
	_pRtmpSrc->setListener(_listener);
	AMFValue metaData(AMF_OBJECT);
	metaData.set("duration", _pParser->getDuration());
	metaData.set("fileSize", 0);
	//todo(xzl) 修复此处
#if 0

	if (_pParser->containVideo()) {
		metaData.set("width", _pParser->getVideoWidth());
		metaData.set("height", _pParser->getVideoHeight());
		metaData.set("videocodecid", "avc1"); //h.264
		metaData.set("videodatarate", 5000);
		metaData.set("framerate", _pParser->getVideoFps());
		makeVideoConfigPkt();
	}
	if (_pParser->containAudio()) {
		metaData.set("audiocodecid", "mp4a"); //aac
		metaData.set("audiodatarate", 160);
		metaData.set("audiosamplerate", _pParser->getAudioSampleRate());
		metaData.set("audiosamplesize", _pParser->getAudioSampleBit());
		metaData.set("audiochannels", _pParser->getAudioChannel());
		metaData.set("stereo", _pParser->getAudioChannel() > 1);
		makeAudioConfigPkt();
	}

#endif
	_pRtmpSrc->onGetMetaData(metaData);
}

} /* namespace mediakit */
