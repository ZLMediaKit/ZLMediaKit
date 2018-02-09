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

#ifdef ENABLE_MP4V2

#include <netinet/in.h>
#include <sys/stat.h>
#include "Common/config.h"
#include "Mp4Maker.h"
#include "MediaRecorder.h"
#include "Util/File.h"
#include "Util/mini.h"
#include "Util/NoticeCenter.h"

using namespace ZL::Util;

namespace ZL {
namespace MediaFile {

string timeStr(const char *fmt) {
	std::tm tm_snapshot;
	auto time = ::time(NULL);
#if defined(WIN32)
	localtime_s(&tm_snapshot, &time); // thread-safe?
	std::ostringstream oss;
	oss << std::put_time(const_cast<std::tm*>(&tm_snapshot), fmt);
	return oss.str();
#else
	localtime_r(&time, &tm_snapshot); // POSIX
	const size_t size = 1024;
	char buffer[size];
	auto success = std::strftime(buffer, size, fmt, &tm_snapshot);
	if (0 == success)
		return string(fmt);
	return buffer;
#endif
}

Mp4Maker::Mp4Maker(const string& strPath,
				   const string &strVhost,
				   const string &strApp,
				   const string &strStreamId,
				   const PlayerBase::Ptr &pPlayer) {
	DebugL << strPath;
	m_pPlayer = pPlayer;
	m_strPath = strPath;

	/////record 业务逻辑//////
	m_info.strAppName = strApp;
	m_info.strStreamId = strStreamId;
	m_info.strVhost = strVhost;
	m_info.strFolder = strPath;
	//----record 业务逻辑----//
}
Mp4Maker::~Mp4Maker() {
	closeFile();
}

void Mp4Maker::inputH264(void *pData, uint32_t ui32Length, uint32_t ui32TimeStamp, int iType){
	switch (iType) {
	case 1: //P
	case 5: { //IDR
		if (m_strLastVideo.size()) {
			int64_t iTimeInc = (int64_t)ui32TimeStamp - (int64_t)m_ui32LastVideoTime;
			iTimeInc = MAX(0,MIN(iTimeInc,500));
			if(iTimeInc == 0 ||  iTimeInc == 500){
				WarnL << "abnormal time stamp increment:" << ui32TimeStamp << " " << m_ui32LastVideoTime;
			}
			_inputH264((char *) m_strLastVideo.data(), m_strLastVideo.size(), iTimeInc, m_iLastVideoType);
		}
		//m_strLastVideo.assign(("\x0\x0\x0\x2\x9\xf0"), 6);
		uint32_t *p = (uint32_t *) pData;
		*p = htonl(ui32Length - 4);
		m_strLastVideo.assign((char *) pData, ui32Length);
		memcpy(pData, "\x00\x00\x00\x01", 4);

		m_ui32LastVideoTime = ui32TimeStamp;
		m_iLastVideoType = iType;
	}
		break;
	default:
		break;
	}
}
void Mp4Maker::inputAAC(void *pData, uint32_t ui32Length, uint32_t ui32TimeStamp){
	if (m_strLastAudio.size()) {
		int64_t iTimeInc = (int64_t)ui32TimeStamp - (int64_t)m_ui32LastAudioTime;
		iTimeInc = MAX(0,MIN(iTimeInc,500));
		if(iTimeInc == 0 ||  iTimeInc == 500){
			WarnL << "abnormal time stamp increment:" << ui32TimeStamp << " " << m_ui32LastAudioTime;
		}
		_inputAAC((char *)m_strLastAudio.data(), m_strLastAudio.size(), iTimeInc);
	}
	m_strLastAudio.assign((char *)pData, ui32Length);
	m_ui32LastAudioTime = ui32TimeStamp;
}

void Mp4Maker::_inputH264(void* pData, uint32_t ui32Length, uint32_t ui32Duration, int iType) {
    GET_CONFIG_AND_REGISTER(uint32_t,recordSec,Config::Record::kFileSecond);

    if(iType == 5 && (m_hMp4 == MP4_INVALID_FILE_HANDLE || m_ticker.elapsedTime() > recordSec * 1000)){
		//在I帧率处新建MP4文件
		//如果文件未创建或者文件超过10分钟则创建新文件
		createFile();
	}
	if (m_hVideo != MP4_INVALID_TRACK_ID) {
		MP4WriteSample(m_hMp4, m_hVideo, (uint8_t *) pData, ui32Length,ui32Duration * 90,0,iType == 5);
	}
}

void Mp4Maker::_inputAAC(void* pData, uint32_t ui32Length, uint32_t ui32Duration) {
    GET_CONFIG_AND_REGISTER(uint32_t,recordSec,Config::Record::kFileSecond);

    if (!m_pPlayer->containVideo() && (m_hMp4 == MP4_INVALID_FILE_HANDLE || m_ticker.elapsedTime() > recordSec * 1000)) {
		//在I帧率处新建MP4文件
		//如果文件未创建或者文件超过10分钟则创建新文件
		createFile();
	}
	if (m_hAudio != MP4_INVALID_TRACK_ID) {
		auto duration = ui32Duration * m_pPlayer->getAudioSampleRate() /1000.0;
		MP4WriteSample(m_hMp4, m_hAudio, (uint8_t*)pData + 7, ui32Length - 7,duration,0,false);
	}
}

void Mp4Maker::createFile() {
	if(!m_pPlayer->isInited()){
		return;
	}
	closeFile();

	auto strDate = timeStr("%Y-%m-%d");
	auto strTime = timeStr("%H-%M-%S");
	auto strFileTmp = m_strPath + strDate + "/." + strTime + ".mp4";
	auto strFile =	m_strPath + strDate + "/" + strTime + ".mp4";

	/////record 业务逻辑//////
	m_info.ui64StartedTime = ::time(NULL);
	m_info.strFileName = strTime + ".mp4";
	m_info.strFilePath = strFile;

    GET_CONFIG_AND_REGISTER(string,appName,Config::Record::kAppName);

    m_info.strUrl = m_info.strVhost + "/"
					+ appName + "/"
					+ m_info.strAppName + "/"
					+ m_info.strStreamId + "/"
					+ strDate + "/"
					+ strTime + ".mp4";
	//----record 业务逻辑----//

	File::createfile_path(strFileTmp.data(), S_IRWXO | S_IRWXG | S_IRWXU);
	m_hMp4 = MP4Create(strFileTmp.data());
	if (m_hMp4 == MP4_INVALID_FILE_HANDLE) {
		WarnL << "创建MP4文件失败:" << strFileTmp;
		return;
	}
	//MP4SetTimeScale(m_hMp4, 90000);
	m_strFileTmp = strFileTmp;
	m_strFile = strFile;
	m_ticker.resetTime();
	if(m_pPlayer->containVideo()){
		auto &sps = m_pPlayer->getSps();
		auto &pps = m_pPlayer->getPps();
		m_hVideo = MP4AddH264VideoTrack(m_hMp4, 90000, MP4_INVALID_DURATION,
				m_pPlayer->getVideoWidth(), m_pPlayer->getVideoHeight(),
				sps[5], sps[6], sps[7], 3);
		if(m_hVideo !=MP4_INVALID_TRACK_ID){
			MP4AddH264SequenceParameterSet(m_hMp4, m_hVideo, (uint8_t *)sps.data() + 4, sps.size() - 4);
			MP4AddH264PictureParameterSet(m_hMp4, m_hVideo, (uint8_t *)pps.data() + 4, pps.size() - 4);
		}else{
			WarnL << "添加视频通道失败:" << strFileTmp;
		}
	}
	if(m_pPlayer->containAudio()){
		m_hAudio = MP4AddAudioTrack(m_hMp4, m_pPlayer->getAudioSampleRate(), MP4_INVALID_DURATION, MP4_MPEG4_AUDIO_TYPE);
		if (m_hAudio != MP4_INVALID_TRACK_ID) {
			auto &cfg =  m_pPlayer->getAudioCfg();
			MP4SetTrackESConfiguration(m_hMp4, m_hAudio,(uint8_t *)cfg.data(), cfg.size());
		}else{
			WarnL << "添加音频通道失败:" << strFileTmp;
		}
	}
}

void Mp4Maker::closeFile() {
	if (m_hMp4 != MP4_INVALID_FILE_HANDLE) {
		{
			TimeTicker();
			MP4Close(m_hMp4,MP4_CLOSE_DO_NOT_COMPUTE_BITRATE);
		}
		rename(m_strFileTmp.data(),m_strFile.data());
		m_hMp4 = MP4_INVALID_FILE_HANDLE;
		m_hVideo = MP4_INVALID_TRACK_ID;
		m_hAudio = MP4_INVALID_TRACK_ID;

		/////record 业务逻辑//////
		m_info.ui64TimeLen = ::time(NULL) - m_info.ui64StartedTime;
		//获取文件大小
		struct stat fileData;
		stat(m_strFile.data(), &fileData);
		m_info.ui64FileSize = fileData.st_size;
		//----record 业务逻辑----//
		NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastRecordMP4,m_info,*this);
	}
}

} /* namespace MediaFile */
} /* namespace ZL */


#endif //ENABLE_MP4V2
