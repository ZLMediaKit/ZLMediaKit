/*
 * RtspToRtmpMediaSource.cpp
 *
 *  Created on: 2016年9月7日
 *      Author: xzl
 */

#include <unistd.h>
#include <string.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "Common/config.h"
#include "Rtmp/Rtmp.h"
#include "RtspToRtmpMediaSource.h"
#include "Util/util.h"
#include "Network/sockutil.h"

using namespace ZL::Util;
using namespace ZL::Network;

namespace ZL {
namespace Rtsp {

#ifdef ENABLE_RTSP2RTMP

RtspToRtmpMediaSource::RtspToRtmpMediaSource(const string &_app,const string &_id,bool bEnableFile) :
		RtspMediaSource(_app,_id),m_bEnableFile(bEnableFile) {
}

RtspToRtmpMediaSource::~RtspToRtmpMediaSource() {

}

void RtspToRtmpMediaSource::regist() {
	RtspMediaSource::regist();
	if (m_pRtmpSrc) {
		m_pRtmpSrc->regist();
	}
}

void RtspToRtmpMediaSource::unregist() {
	RtspMediaSource::unregist();
	if (m_pRtmpSrc) {
		m_pRtmpSrc->unregist();
	}
}
void RtspToRtmpMediaSource::onGetH264(const H264Frame& frame) {
	if(m_pRecorder){
		m_pRecorder->inputH264((char *) frame.data.data(), frame.data.size(), frame.timeStamp, frame.type);
	}
	uint8_t nal_type = frame.data[4] & 0x1F;
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
	m_rtmpPkt.strBuf.clear();
	m_rtmpPkt.strBuf.push_back(flags);
	m_rtmpPkt.strBuf.push_back(!is_config);
	m_rtmpPkt.strBuf.append("\x0\x0\x0", 3);
	uint32_t size = frame.data.size() - 4;
	size = htonl(size);
	m_rtmpPkt.strBuf.append((char *) &size, 4);
	m_rtmpPkt.strBuf.append(&frame.data[4], frame.data.size() - 4);

	m_rtmpPkt.bodySize = m_rtmpPkt.strBuf.size();
	m_rtmpPkt.chunkId = CHUNK_MEDIA;
	m_rtmpPkt.streamId = STREAM_MEDIA;
	m_rtmpPkt.timeStamp = frame.timeStamp;
	m_rtmpPkt.typeId = MSG_VIDEO;
	m_pRtmpSrc->onGetMedia(m_rtmpPkt);
}
void RtspToRtmpMediaSource::makeVideoConfigPkt() {
	int8_t flags = 7; //h.264
	flags |= (FLV_KEY_FRAME << 4);
	bool is_config = true;

	m_rtmpPkt.strBuf.clear();
//////////header
	m_rtmpPkt.strBuf.push_back(flags);
	m_rtmpPkt.strBuf.push_back(!is_config);
	m_rtmpPkt.strBuf.append("\x0\x0\x0", 3);

////////////sps
	m_rtmpPkt.strBuf.push_back(1); // version
	string m_sps = m_pParser->getSps().substr(4);
	string m_pps = m_pParser->getPps().substr(4);
	//DebugL<<hexdump(m_sps.data(), m_sps.size());
	m_rtmpPkt.strBuf.push_back(m_sps[1]); // profile
	m_rtmpPkt.strBuf.push_back(m_sps[2]); // compat
	m_rtmpPkt.strBuf.push_back(m_sps[3]); // level
	m_rtmpPkt.strBuf.push_back(0xff); // 6 bits reserved + 2 bits nal size length - 1 (11)
	m_rtmpPkt.strBuf.push_back(0xe1); // 3 bits reserved + 5 bits number of sps (00001)
	uint16_t size = m_sps.size();
	size = htons(size);
	m_rtmpPkt.strBuf.append((char *) &size, 2);
	m_rtmpPkt.strBuf.append(m_sps);

/////////////pps
	m_rtmpPkt.strBuf.push_back(1); // version
	size = m_pps.size();
	size = htons(size);
	m_rtmpPkt.strBuf.append((char *) &size, 2);
	m_rtmpPkt.strBuf.append(m_pps);

	m_rtmpPkt.bodySize = m_rtmpPkt.strBuf.size();
	m_rtmpPkt.chunkId = CHUNK_MEDIA;
	m_rtmpPkt.streamId = STREAM_MEDIA;
	m_rtmpPkt.timeStamp = 0;
	m_rtmpPkt.typeId = MSG_VIDEO;
	m_pRtmpSrc->onGetMedia(m_rtmpPkt);
}

void RtspToRtmpMediaSource::onGetAdts(const AdtsFrame& frame) {
	if(m_pRecorder){
		m_pRecorder->inputAAC((char *) frame.data, frame.aac_frame_length, frame.timeStamp);
	}

	m_rtmpPkt.strBuf.clear();
//////////header
	uint8_t is_config = false;
	m_rtmpPkt.strBuf.push_back(m_ui8AudioFlags);
	m_rtmpPkt.strBuf.push_back(!is_config);
	m_rtmpPkt.strBuf.append((char *) frame.data + 7, frame.aac_frame_length - 7);

	m_rtmpPkt.bodySize = m_rtmpPkt.strBuf.size();
	m_rtmpPkt.chunkId = CHUNK_MEDIA;
	m_rtmpPkt.streamId = STREAM_MEDIA;
	m_rtmpPkt.timeStamp = frame.timeStamp;
	m_rtmpPkt.typeId = MSG_AUDIO;
	m_pRtmpSrc->onGetMedia(m_rtmpPkt);
}

void RtspToRtmpMediaSource::makeAudioConfigPkt() {
	uint8_t flvStereoOrMono = (m_pParser->getAudioChannel() > 1);
	uint8_t flvSampleRate;
	switch (m_pParser->getAudioSampleRate()) {
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
	uint8_t flvSampleBit = m_pParser->getAudioSampleBit() == 16;
	uint8_t flvAudioType = 10; //aac

	m_ui8AudioFlags = (flvAudioType << 4) | (flvSampleRate << 2) | (flvSampleBit << 1) | flvStereoOrMono;

	m_rtmpPkt.strBuf.clear();
//////////header
	uint8_t is_config = true;
	m_rtmpPkt.strBuf.push_back(m_ui8AudioFlags);
	m_rtmpPkt.strBuf.push_back(!is_config);
	m_rtmpPkt.strBuf.append(m_pParser->getAudioCfg());

	m_rtmpPkt.bodySize = m_rtmpPkt.strBuf.size();
	m_rtmpPkt.chunkId = CHUNK_MEDIA;
	m_rtmpPkt.streamId = STREAM_MEDIA;
	m_rtmpPkt.timeStamp = 0;
	m_rtmpPkt.typeId = MSG_AUDIO;
	m_pRtmpSrc->onGetMedia(m_rtmpPkt);
}

void RtspToRtmpMediaSource::makeMetaData() {
	m_pRtmpSrc.reset(new RtmpMediaSource(getApp(),getId()));
	m_pRtmpSrc->setOnSeek(m_onSeek);
	m_pRtmpSrc->setOnStamp(m_onStamp);
	AMFValue metaData(AMF_OBJECT);
	metaData.set("duration", m_pParser->getDuration());
	metaData.set("fileSize", 0);
	if (m_pParser->containVideo()) {
		metaData.set("width", m_pParser->getVideoWidth());
		metaData.set("height", m_pParser->getVideoHeight());
		metaData.set("videocodecid", 7); //h.264
		metaData.set("videodatarate", 5000);
		metaData.set("framerate", m_pParser->getVideoFps());
		makeVideoConfigPkt();
	}
	if (m_pParser->containAudio()) {
		metaData.set("audiocodecid", 10); //aac
		metaData.set("audiodatarate", 160);
		metaData.set("audiosamplerate", m_pParser->getAudioSampleRate());
		metaData.set("audiosamplesize", m_pParser->getAudioSampleBit());
		metaData.set("audiochannels", m_pParser->getAudioChannel());
		metaData.set("stereo", m_pParser->getAudioChannel() > 1);
		makeAudioConfigPkt();
	}

	m_pRtmpSrc->onGetMetaData(metaData);
}
#endif //ENABLE_RTSP2RTMP
} /* namespace Rtsp */
} /* namespace ZL */
