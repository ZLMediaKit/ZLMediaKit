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

#include <sys/stat.h>
#include "Common/config.h"
#include "Mp4Maker.h"
#include "MediaRecorder.h"
#include "Util/File.h"
#include "Util/mini.h"
#include "Util/util.h"
#include "Util/NoticeCenter.h"
#include "Extension/H264.h"
#include "Extension/AAC.h"

using namespace toolkit;

namespace mediakit {

string timeStr(const char *fmt) {
	std::tm tm_snapshot;
	auto time = ::time(NULL);
#if defined(_WIN32)
	localtime_s(&tm_snapshot, &time); // thread-safe
#else
	localtime_r(&time, &tm_snapshot); // POSIX
#endif
	const size_t size = 1024;
	char buffer[size];
	auto success = std::strftime(buffer, size, fmt, &tm_snapshot);
	if (0 == success)
		return string(fmt);
	return buffer;
}

Mp4Maker::Mp4Maker(const string& strPath,
				   const string &strVhost,
				   const string &strApp,
				   const string &strStreamId) {
	DebugL << strPath;
	_strPath = strPath;

	/////record 业务逻辑//////
	_info.strAppName = strApp;
	_info.strStreamId = strStreamId;
	_info.strVhost = strVhost;
	_info.strFolder = strPath;
	//----record 业务逻辑----//
}
Mp4Maker::~Mp4Maker() {
	closeFile();
}

void Mp4Maker::inputH264(void *pData, uint32_t ui32Length, uint32_t ui32TimeStamp){
	auto iType = H264_TYPE(((uint8_t*)pData)[0]);
	switch (iType) {
	case H264Frame::NAL_B_P: //P
	case H264Frame::NAL_IDR: { //IDR
		if (_strLastVideo.size()) {
			int64_t iTimeInc = (int64_t)ui32TimeStamp - (int64_t)_ui32LastVideoTime;
			iTimeInc = MAX(0,MIN(iTimeInc,500));
			if(iTimeInc == 0 ||  iTimeInc == 500){
				WarnL << "abnormal time stamp increment:" << ui32TimeStamp << " " << _ui32LastVideoTime;
			}
			inputH264_l((char *) _strLastVideo.data(), _strLastVideo.size(), iTimeInc);
		}

		uint32_t prefixe  = htonl(ui32Length);
		_strLastVideo.assign((char *) &prefixe, 4);
		_strLastVideo.append((char *)pData,ui32Length);

		_ui32LastVideoTime = ui32TimeStamp;
	}
		break;
	default:
		break;
	}
}
void Mp4Maker::inputAAC(void *pData, uint32_t ui32Length, uint32_t ui32TimeStamp){
	if (_strLastAudio.size()) {
		int64_t iTimeInc = (int64_t)ui32TimeStamp - (int64_t)_ui32LastAudioTime;
		iTimeInc = MAX(0,MIN(iTimeInc,500));
		if(iTimeInc == 0 ||  iTimeInc == 500){
			WarnL << "abnormal time stamp increment:" << ui32TimeStamp << " " << _ui32LastAudioTime;
		}
		inputAAC_l((char *) _strLastAudio.data(), _strLastAudio.size(), iTimeInc);
	}
	_strLastAudio.assign((char *)pData, ui32Length);
	_ui32LastAudioTime = ui32TimeStamp;
}

void Mp4Maker::inputH264_l(void *pData, uint32_t ui32Length, uint32_t ui32Duration) {
    GET_CONFIG_AND_REGISTER(uint32_t,recordSec,Record::kFileSecond);
	auto iType =  H264_TYPE(((uint8_t*)pData)[4]);
	if(iType == H264Frame::NAL_IDR && (_hMp4 == MP4_INVALID_FILE_HANDLE || _ticker.elapsedTime() > recordSec * 1000)){
		//在I帧率处新建MP4文件
		//如果文件未创建或者文件超过10分钟则创建新文件
		createFile();
	}
	if (_hVideo != MP4_INVALID_TRACK_ID) {
		MP4WriteSample(_hMp4, _hVideo, (uint8_t *) pData, ui32Length,ui32Duration * 90,0,iType == 5);
	}
}

void Mp4Maker::inputAAC_l(void *pData, uint32_t ui32Length, uint32_t ui32Duration) {
    GET_CONFIG_AND_REGISTER(uint32_t,recordSec,Record::kFileSecond);

    if (!_haveVideo && (_hMp4 == MP4_INVALID_FILE_HANDLE || _ticker.elapsedTime() > recordSec * 1000)) {
		//在I帧率处新建MP4文件
		//如果文件未创建或者文件超过10分钟则创建新文件
		createFile();
	}
	if (_hAudio != MP4_INVALID_TRACK_ID) {
		auto duration = ui32Duration * _audioSampleRate /1000.0;
		MP4WriteSample(_hMp4, _hAudio, (uint8_t*)pData, ui32Length,duration,0,false);
	}
}

void Mp4Maker::createFile() {
	closeFile();

	auto strDate = timeStr("%Y-%m-%d");
	auto strTime = timeStr("%H-%M-%S");
	auto strFileTmp = _strPath + strDate + "/." + strTime + ".mp4";
	auto strFile =	_strPath + strDate + "/" + strTime + ".mp4";

	/////record 业务逻辑//////
	_info.ui64StartedTime = ::time(NULL);
	_info.strFileName = strTime + ".mp4";
	_info.strFilePath = strFile;

    GET_CONFIG_AND_REGISTER(string,appName,Record::kAppName);

    _info.strUrl = _info.strVhost + "/"
					+ appName + "/"
					+ _info.strAppName + "/"
					+ _info.strStreamId + "/"
					+ strDate + "/"
					+ strTime + ".mp4";
	//----record 业务逻辑----//

#if !defined(_WIN32)
	File::createfile_path(strFileTmp.data(), S_IRWXO | S_IRWXG | S_IRWXU);
#else
	File::createfile_path(strFileTmp.data(), 0);
#endif
	_hMp4 = MP4Create(strFileTmp.data());
	if (_hMp4 == MP4_INVALID_FILE_HANDLE) {
		WarnL << "创建MP4文件失败:" << strFileTmp;
		return;
	}
	//MP4SetTimeScale(_hMp4, 90000);
	_strFileTmp = strFileTmp;
	_strFile = strFile;
	_ticker.resetTime();

	auto videoTrack = dynamic_pointer_cast<H264Track>(getTrack(TrackVideo));
	if(videoTrack){
		auto &sps = videoTrack->getSps();
		auto &pps = videoTrack->getPps();
		_hVideo = MP4AddH264VideoTrack(_hMp4,
									   90000,
									   MP4_INVALID_DURATION,
									   videoTrack->getVideoWidth(),
									   videoTrack->getVideoHeight(),
									   sps[1],
									   sps[2],
									   sps[3],
									   3);
		if(_hVideo != MP4_INVALID_TRACK_ID){
			MP4AddH264SequenceParameterSet(_hMp4, _hVideo, (uint8_t *)sps.data(), sps.size());
			MP4AddH264PictureParameterSet(_hMp4, _hVideo, (uint8_t *)pps.data(), pps.size());
		}else{
			WarnL << "添加视频通道失败:" << strFileTmp;
		}
	}

	auto audioTrack = dynamic_pointer_cast<AACTrack>(getTrack(TrackAudio));
	if(audioTrack){
		_audioSampleRate = audioTrack->getAudioSampleRate();
		_hAudio = MP4AddAudioTrack(_hMp4, _audioSampleRate, MP4_INVALID_DURATION, MP4_MPEG4_AUDIO_TYPE);
		if (_hAudio != MP4_INVALID_TRACK_ID) {
			auto &cfg =  audioTrack->getAacCfg();
			MP4SetTrackESConfiguration(_hMp4, _hAudio,(uint8_t *)cfg.data(), cfg.size());
		}else{
			WarnL << "添加音频通道失败:" << strFileTmp;
		}
	}
}

void Mp4Maker::closeFile() {
	if (_hMp4 != MP4_INVALID_FILE_HANDLE) {
		{
			TimeTicker();
			MP4Close(_hMp4,MP4_CLOSE_DO_NOT_COMPUTE_BITRATE);
		}
		rename(_strFileTmp.data(),_strFile.data());
		_hMp4 = MP4_INVALID_FILE_HANDLE;
		_hVideo = MP4_INVALID_TRACK_ID;
		_hAudio = MP4_INVALID_TRACK_ID;

		/////record 业务逻辑//////
		_info.ui64TimeLen = ::time(NULL) - _info.ui64StartedTime;
		//获取文件大小
		struct stat fileData;
		stat(_strFile.data(), &fileData);
		_info.ui64FileSize = fileData.st_size;
		//----record 业务逻辑----//
		NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastRecordMP4,_info,*this);
	}
}

void Mp4Maker::onTrackFrame(const Frame::Ptr &frame) {
	switch (frame->getCodecId()){
		case CodecH264:{
			inputH264(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize(),frame->stamp());
		}
			break;
		case CodecAAC:{
			inputAAC(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize(),frame->stamp());
		}
			break;

		default:
			break;
	}
}

void Mp4Maker::onAllTrackReady() {
	_haveVideo = getTrack(TrackVideo).operator bool();
}

} /* namespace mediakit */


#endif //ENABLE_MP4V2
