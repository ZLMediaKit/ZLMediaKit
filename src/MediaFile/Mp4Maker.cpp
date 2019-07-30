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

#ifdef ENABLE_MP4V2
#include <ctime>
#include <sys/stat.h>
#include "Common/config.h"
#include "Mp4Maker.h"
#include "MediaRecorder.h"
#include "Util/File.h"
#include "Util/mini.h"
#include "Util/util.h"
#include "Util/NoticeCenter.h"
#include "Extension/H264.h"
#include "Extension/H265.h"
#include "Extension/AAC.h"
#include "Thread/WorkThreadPool.h"


#ifdef MP4_H265RECORD
#include "mov-buffer.h"
#include "mov-format.h"

#if defined(_WIN32) || defined(_WIN64)
#define fseek64 _fseeki64
#define ftell64 _ftelli64
#else
#define fseek64 fseek
#define ftell64 ftell
#endif

static int mov_file_read(void* fp, void* data, uint64_t bytes)
{
    if (bytes == fread(data, 1, bytes, (FILE*)fp))
        return 0;
	return 0 != ferror((FILE*)fp) ? ferror((FILE*)fp) : -1 /*EOF*/;
}

static int mov_file_write(void* fp, const void* data, uint64_t bytes)
{
	return bytes == fwrite(data, 1, bytes, (FILE*)fp) ? 0 : ferror((FILE*)fp);
}

static int mov_file_seek(void* fp, uint64_t offset)
{
	return fseek64((FILE*)fp, offset, SEEK_SET);
}

static uint64_t mov_file_tell(void* fp)
{
	return ftell64((FILE*)fp);
}

const struct mov_buffer_t* mov_file_buffer(void)
{
	static struct mov_buffer_t s_io = {
		mov_file_read,
		mov_file_write,
		mov_file_seek,
		mov_file_tell,
	};
	return &s_io;
}
#endif

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

void Mp4Maker::inputH265(void *pData, uint32_t ui32Length, uint32_t ui32TimeStamp){
#ifdef MP4_H265RECORD
	auto iType = H265_TYPE(((uint8_t*)pData)[4]);
	if (iType <= 19 ){
		if (_strLastVideo.size() && iType == 19){
			_strLastVideo.append((char *)pData,ui32Length);
			inputH265_l((char *) _strLastVideo.data(), _strLastVideo.size(), ui32TimeStamp);
			_strLastVideo = "";
			_ui32LastVideoTime = ui32TimeStamp;
		}else
			inputH265_l((char *) pData, ui32Length, ui32TimeStamp);				
	}else{
		_strLastVideo.append((char *)pData,ui32Length);
		_ui32LastVideoTime = ui32TimeStamp;
	}
#endif	
}

void Mp4Maker::inputAAC(void *pData, uint32_t ui32Length, uint32_t ui32TimeStamp){
#ifdef MP4_H265RECORD
	if (_h265Record){
		inputAAC_l((char *) pData, ui32Length, ui32TimeStamp);
	}else
#endif
	{
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



}

void Mp4Maker::inputH264_l(void *pData, uint32_t ui32Length, uint32_t ui32Duration) {
    GET_CONFIG(uint32_t,recordSec,Record::kFileSecond);
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

void Mp4Maker::inputH265_l(void *pData, uint32_t ui32Length, uint32_t ui32TimeStamp) {
    GET_CONFIG(uint32_t,recordSec,Record::kFileSecond);
	
#ifdef MP4_H265RECORD
	int32_t compositionTime;
	auto iType =  H265_TYPE(((uint8_t*)pData)[4]);
	if( iType >= H265Frame::NAL_IDR_W_RADL && (_movH265info.pMov == NULL || _ticker.elapsedTime() > recordSec * 1000)){
		//在I帧率处新建MP4文件
		//如果文件未创建或者文件超过10分钟则创建新文件
		_h265Record = 1;
		createFile();
	}

	char *pNualData = (char *)pData;
	if (/*iType <= 31 && */_movH265info.pMov!=NULL){
		int vcl;
		//media-server新版的api使用h265_annexbtomp4
		//int n = h265_annexbtomp4(&_movH265info.hevc, pData, ui32Length, _sBbuffer, sizeof(_sBbuffer), &vcl);
		int n = hevc_annexbtomp4(&_movH265info.hevc, pData, ui32Length, _sBbuffer, sizeof(_sBbuffer));
		if (_movH265info.videoTrack < 0){
			if (_movH265info.hevc.numOfArrays < 1){
				return; // waiting for vps/sps/pps
			}
			
			uint8_t sExtraData[64 * 1024];
			int extraDataSize = mpeg4_hevc_decoder_configuration_record_save(&_movH265info.hevc, sExtraData, sizeof(sExtraData));
			if (extraDataSize <= 0){
				// invalid HVCC
				return;
			}

			// TODO: waiting for key frame ???
			_movH265info.videoTrack = mov_writer_add_video(_movH265info.pMov, MOV_OBJECT_HEVC, _movH265info.width, _movH265info.height, sExtraData, extraDataSize);
			if (_movH265info.videoTrack < 0)
				return;
		}
		mov_writer_write(_movH265info.pMov,
							_movH265info.videoTrack, 
							_sBbuffer, 
							n, 
							ui32TimeStamp, 
							ui32TimeStamp, 
							(iType >= 16 && iType <= 23) ? MOV_AV_FLAG_KEYFREAME : 0 );		
//		mov_writer_write(_movH265info.pMov, _movH265info.videoTrack, _sBbuffer, n, ui32TimeStamp, ui32TimeStamp, 1 == vcl ? MOV_AV_FLAG_KEYFREAME : 0);
	}
#endif

}

void Mp4Maker::inputAAC_l(void *pData, uint32_t ui32Length, uint32_t ui32Duration) {
    GET_CONFIG(uint32_t,recordSec,Record::kFileSecond);
#ifdef MP4_H265RECORD
	if ( _h265Record )
	{
		if (!_haveVideo && (_movH265info.pMov == NULL || _ticker.elapsedTime() > recordSec * 1000)) {
			createFile();
		}

		if (-1 != _movH265info.audioTrack && _movH265info.pMov != NULL){
			mov_writer_write(_movH265info.pMov, _movH265info.audioTrack,  (uint8_t*)pData, ui32Length, ui32Duration, ui32Duration, 0);
		}
	}else
#endif	
	{
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

    GET_CONFIG(string,appName,Record::kAppName);

    _info.strUrl = appName + "/"
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

#ifdef MP4_H265RECORD
	if ( _h265Record ){
		memset(&_movH265info, 0, sizeof(_movH265info));
		_movH265info.videoTrack = -1;	
		_movH265info.audioTrack = -1;
		_movH265info.width = 0;
		_movH265info.height = 0;
		_movH265info.ptr = NULL;
		_movH265info.pFile = fopen(strFileTmp.data(), "wb+");
		_movH265info.pMov  = mov_writer_create(mov_file_buffer(), _movH265info.pFile, 0/*MOV_FLAG_FASTSTART*/);
	}else
#endif	
	{
		_hMp4 = MP4Create(strFileTmp.data(),MP4_CREATE_64BIT_DATA);
		if (_hMp4 == MP4_INVALID_FILE_HANDLE) {
			WarnL << "创建MP4文件失败:" << strFileTmp;
			return;
		}
	}

	//MP4SetTimeScale(_hMp4, 90000);
	_strFileTmp = strFileTmp;
	_strFile = strFile;
	_ticker.resetTime();

	if ( _h265Record ){
		auto videoTrack = dynamic_pointer_cast<H265Track>(getTrack(TrackVideo));
#ifdef MP4_H265RECORD
		if(videoTrack){
			_movH265info.width = videoTrack->getVideoWidth();
			_movH265info.height = videoTrack->getVideoHeight();
		}
#endif
		
	}else	{
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
		
	}

	auto audioTrack = dynamic_pointer_cast<AACTrack>(getTrack(TrackAudio));
	if(audioTrack){
		_audioSampleRate = audioTrack->getAudioSampleRate();
		_audioChannel = audioTrack->getAudioChannel();
#ifdef MP4_H265RECORD
		uint8_t extra_data[64 * 1024];
		if ( _h265Record ){
			_movH265info.audioTrack = mov_writer_add_audio(_movH265info.pMov, MOV_OBJECT_AAC, _audioChannel, 16, _audioSampleRate, audioTrack->getAacCfg().data(), 2);
			if (-1 == _movH265info.audioTrack) 
				WarnL << "添加音频通道失败:" << strFileTmp;
		}else
#endif		
		{
			_hAudio = MP4AddAudioTrack(_hMp4, _audioSampleRate, MP4_INVALID_DURATION, MP4_MPEG4_AUDIO_TYPE);
			if (_hAudio != MP4_INVALID_TRACK_ID) {
				auto &cfg =  audioTrack->getAacCfg();
				MP4SetTrackESConfiguration(_hMp4, _hAudio,(uint8_t *)cfg.data(), cfg.size());
			}else{
				WarnL << "添加音频通道失败:" << strFileTmp;
			}
		}
	}
}

void Mp4Maker::asyncClose() {

//	auto hMp4 = (_h265Record==0)?_hMp4:_movH265info.pMov;
	auto strFileTmp = _strFileTmp;
	auto strFile = _strFile;
	auto info = _info;
	
	int h265Record = _h265Record;
#ifdef MP4_H265RECORD
	FILE *pFile = (_h265Record)?_movH265info.pFile:NULL;
	void * hMp4 = (_h265Record)?(void*)_movH265info.pMov:(void*)_hMp4;
#else
	auto hMp4 = _hMp4;
	FILE *pFile = NULL;
#endif
	WorkThreadPool::Instance().getExecutor()->async([hMp4,strFileTmp,strFile,info,pFile,h265Record]() {
		//获取文件录制时间，放在MP4Close之前是为了忽略MP4Close执行时间
		const_cast<Mp4Info&>(info).ui64TimeLen = ::time(NULL) - info.ui64StartedTime;
		//MP4Close非常耗时，所以要放在后台线程执行
		
#ifdef MP4_H265RECORD
		if (h265Record){
			mov_writer_destroy((mov_writer_t*)hMp4);			
			fclose(pFile);
		}else
#endif
		{	
			MP4Close(hMp4,MP4_CLOSE_DO_NOT_COMPUTE_BITRATE);
		}
		//临时文件名改成正式文件名，防止mp4未完成时被访问
		rename(strFileTmp.data(),strFile.data());
		//获取文件大小
		struct stat fileData;
		stat(strFile.data(), &fileData);
		const_cast<Mp4Info&>(info).ui64FileSize = fileData.st_size;
		/////record 业务逻辑//////
		NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastRecordMP4,info);
	});
}

void Mp4Maker::closeFile() {
#ifdef MP4_H265RECORD
	if (_h265Record){
		if (_movH265info.pMov != NULL) {	
			asyncClose();
		}
	}else
#endif
	{
		if (_hMp4 != MP4_INVALID_FILE_HANDLE) {
			asyncClose();
			_hMp4 = MP4_INVALID_FILE_HANDLE;
			_hVideo = MP4_INVALID_TRACK_ID;
			_hAudio = MP4_INVALID_TRACK_ID;
		}
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
		case CodecH265:{
			inputH265(frame->data() , frame->size(),frame->stamp());
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
