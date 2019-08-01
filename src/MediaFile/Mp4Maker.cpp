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

#ifdef ENABLE_MP4RECORD
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


#include "mov-buffer.h"
#include "mov-format.h"



using namespace toolkit;

namespace mediakit {

#if defined(_WIN32) || defined(_WIN64)
#define fseek64 _fseeki64
#define ftell64 _ftelli64
#else
#define fseek64 fseek
#define ftell64 ftell
#endif

static int movfileRead(void* fp, void* data, uint64_t bytes)
{
    if (bytes == fread(data, 1, bytes, (FILE*)fp))
        return 0;
	return 0 != ferror((FILE*)fp) ? ferror((FILE*)fp) : -1 /*EOF*/;
}

static int movfileWrite(void* fp, const void* data, uint64_t bytes)
{
	return bytes == fwrite(data, 1, bytes, (FILE*)fp) ? 0 : ferror((FILE*)fp);
}

static int movfileSeek(void* fp, uint64_t offset)
{
	return fseek64((FILE*)fp, offset, SEEK_SET);
}

static uint64_t movfileTell(void* fp)
{
	return ftell64((FILE*)fp);
}

const struct mov_buffer_t* movfileBuffer(void)
{
	static struct mov_buffer_t s_io = {
		movfileRead,
		movfileWrite,
		movfileSeek,
		movfileTell,
	};
	return &s_io;
}


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

	memset(&_movH265info, 0, sizeof(_movH265info));
	_movH265info.videoTrack = -1;	
	_movH265info.audioTrack = -1;
	
	//----record 业务逻辑----//
}
Mp4Maker::~Mp4Maker() {
	closeFile();
}

void Mp4Maker::inputH264(void *pData, uint32_t ui32Length, uint32_t ui32TimeStamp){
	auto iType = H264_TYPE(((uint8_t*)pData)[4]);
	
	if (H264Frame::NAL_B_P <= iType && iType <= H264Frame::NAL_IDR){	

		int64_t iTimeInc = (int64_t)ui32TimeStamp - (int64_t)_ui32LastVideoTime;
		iTimeInc = MAX(0,MIN(iTimeInc,500));
		if( (iTimeInc == 0 ||  iTimeInc == 500) && H264Frame::NAL_IDR != iType){
			WarnL << "abnormal time stamp increment:" << ui32TimeStamp << " " << _ui32LastVideoTime;
		}		
	
		if ( _strLastVideo.size() ){
			//如果出现SPS PPS黏连的帧,那么在这里先处理
			inputH264_l((char *) _strLastVideo.data(), _strLastVideo.size(), _ui32LastVideoTime); 	
			_strLastVideo = "";			
		}				
		inputH264_l((char *) pData, ui32Length, ui32TimeStamp); 	
		_ui32LastVideoTime = ui32TimeStamp;
	}else{
		//SPS PPS 进入等待组合为一帧
		_strLastVideo.append((char *)pData,ui32Length);
		_ui32LastVideoTime = ui32TimeStamp;
	}
}

void Mp4Maker::inputH265(void *pData, uint32_t ui32Length, uint32_t ui32TimeStamp){

	auto iType = H265_TYPE(((uint8_t*)pData)[4]);
	if (iType <= H265Frame::NAL_IDR_W_RADL ){
		
		int64_t iTimeInc = (int64_t)ui32TimeStamp - (int64_t)_ui32LastVideoTime;
		iTimeInc = MAX(0,MIN(iTimeInc,500));
		if((iTimeInc == 0 ||  iTimeInc == 500) && H265Frame::NAL_IDR_W_RADL != iType){
			WarnL << "abnormal time stamp increment:" << ui32TimeStamp << " " << _ui32LastVideoTime;
		}	
		
		if ( _strLastVideo.size() ){
			//如果出现SPS PPS VPS黏连的帧,那么在这里先处理
			inputH265_l((char *) _strLastVideo.data(), _strLastVideo.size(), _ui32LastVideoTime); 	
			_strLastVideo = "";
		}
		inputH265_l((char *) pData, ui32Length, ui32TimeStamp);	
		_ui32LastVideoTime = ui32TimeStamp;
	}else{
		_strLastVideo.append((char *)pData,ui32Length);
		_ui32LastVideoTime = ui32TimeStamp;
	}
}

void Mp4Maker::inputAAC(void *pData, uint32_t ui32Length, uint32_t ui32TimeStamp){	
	int64_t iTimeInc = (int64_t)ui32TimeStamp - (int64_t)_ui32LastAudioTime;
	iTimeInc = MAX(0,MIN(iTimeInc,500));
	if(iTimeInc == 0 ||  iTimeInc == 500){
		WarnL << "abnormal time stamp increment:" << ui32TimeStamp << " " << _ui32LastAudioTime;
	}		
	inputAAC_l((char *) pData, ui32Length, ui32TimeStamp);
	_ui32LastAudioTime = ui32TimeStamp;
}

void Mp4Maker::inputH264_l(void *pData, uint32_t ui32Length, uint32_t ui32TimeStamp) {
    GET_CONFIG(uint32_t,recordSec,Record::kFileSecond);
	auto iType =  H264_TYPE(((uint8_t*)pData)[4]);
	int32_t compositionTime;
	if( iType >= H264Frame::NAL_IDR && (_movH265info.pMov == NULL || _ticker.elapsedTime() > recordSec * 1000)){
		//在I帧率处新建MP4文件
		//如果文件未创建或者文件超过10分钟则创建新文件
		//每一个录制的MP4文件时间戳都要从0开始		
		_startPts = ui32TimeStamp;		
		createFile();	
	}

	char *pNualData = (char *)pData;
	if (_movH265info.pMov!=NULL){
		int vcl;		
		if (_movH265info.videoTrack < 0){
			//解析解析SPS PPS,未添加track的时候执行
			int n = h264_annexbtomp4(&_movH265info.avc, pData, ui32Length, _sBbuffer, sizeof(_sBbuffer), &vcl);
			if (_movH265info.avc.nb_sps < 1 || _movH265info.avc.nb_pps < 1){
				return; // waiting for sps/pps
			}
			
			uint8_t sExtraData[64 * 1024];
			int extraDataSize = mpeg4_avc_decoder_configuration_record_save(&_movH265info.avc, sExtraData, sizeof(sExtraData));
			if (extraDataSize <= 0){
				// invalid HVCC
				return;
			}

			// TODO: waiting for key frame ???
			_movH265info.videoTrack = mov_writer_add_video(_movH265info.pMov,
															MOV_OBJECT_H264, 
															_movH265info.width, 
															_movH265info.height, 
															sExtraData, 
															extraDataSize);
			return; 		
		}
		if ( iType <= H264Frame::NAL_IDR ){
			uint8_t *ptr = (uint8_t*)pData;
			ptr[0] = (uint8_t)((ui32Length-4 >> 24) & 0xFF);
			ptr[1] = (uint8_t)((ui32Length-4 >> 16) & 0xFF);
			ptr[2] = (uint8_t)((ui32Length-4 >> 8) & 0xFF);
			ptr[3] = (uint8_t)((ui32Length-4 >> 0) & 0xFF);
			uint32_t ui32Pts = ui32TimeStamp < _movH265info.startPts ? 0 : ui32TimeStamp-_movH265info.startPts;
			mov_writer_write(_movH265info.pMov, 
								_movH265info.videoTrack, pData, ui32Length, 
								ui32Pts, 
								ui32Pts, 
								iType == H264Frame::NAL_IDR ? MOV_AV_FLAG_KEYFREAME : 0);
		}
	}

}

void Mp4Maker::inputH265_l(void *pData, uint32_t ui32Length, uint32_t ui32TimeStamp) {
    GET_CONFIG(uint32_t,recordSec,Record::kFileSecond);
	
	int32_t compositionTime;
	auto iType =  H265_TYPE(((uint8_t*)pData)[4]);
	if( iType >= H265Frame::NAL_IDR_W_RADL && (_movH265info.pMov == NULL || _ticker.elapsedTime() > recordSec * 1000)){
		_h265Record = 1;
		//每一个录制的MP4文件时间戳都要从0开始
		_startPts = ui32TimeStamp;
		
		//在I帧率处新建MP4文件
		//如果文件未创建或者文件超过最长时间则创建新文件
		createFile();
	}

	char *pNualData = (char *)pData;
	if (_movH265info.pMov!=NULL){
		int vcl;
		if (_movH265info.videoTrack < 0){
			//解析解析VPS SPS PPS,未添加track的时候执行
			int n = h265_annexbtomp4(&_movH265info.hevc, pData, ui32Length, _sBbuffer, sizeof(_sBbuffer), &vcl);
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

			return;			
		}
		if ( iType <= H265Frame::NAL_IDR_W_RADL )
		{
			uint8_t *ptr = (uint8_t*)pData;
			ptr[0] = (uint8_t)((ui32Length-4 >> 24) & 0xFF);
			ptr[1] = (uint8_t)((ui32Length-4 >> 16) & 0xFF);
			ptr[2] = (uint8_t)((ui32Length-4 >> 8) & 0xFF);
			ptr[3] = (uint8_t)((ui32Length-4 >> 0) & 0xFF);

			uint32_t ui32Pts = ui32TimeStamp < _movH265info.startPts ? 0 : ui32TimeStamp-_movH265info.startPts;
			mov_writer_write(_movH265info.pMov,
							_movH265info.videoTrack,
							pData, 
							ui32Length, 
							ui32Pts, 
							ui32Pts, 
							iType == H265Frame::NAL_IDR_W_RADL ? MOV_AV_FLAG_KEYFREAME : 0);
		}
	}

}

void Mp4Maker::inputAAC_l(void *pData, uint32_t ui32Length, uint32_t ui32TimeStamp) {
    GET_CONFIG(uint32_t,recordSec,Record::kFileSecond);
	if (!_haveVideo && (_movH265info.pMov == NULL || _ticker.elapsedTime() > recordSec * 1000)) {			
		_startPts = ui32TimeStamp;
		createFile();			
	}

	if (-1 != _movH265info.audioTrack && _movH265info.pMov != NULL){	

		uint32_t ui32Pts = ui32TimeStamp < _movH265info.startPts ? 0 : ui32TimeStamp-_movH265info.startPts;
		mov_writer_write(_movH265info.pMov, _movH265info.audioTrack,  (uint8_t*)pData, ui32Length, ui32Pts, ui32Pts, 0);		
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

	memset(&_movH265info, 0, sizeof(_movH265info));
	_movH265info.videoTrack = -1;	
	_movH265info.audioTrack = -1;
	_movH265info.width = 0;
	_movH265info.height = 0;
	_movH265info.pFile = fopen(strFileTmp.data(), "wb+");
	_movH265info.startPts = _startPts;
	_movH265info.pMov  = mov_writer_create(movfileBuffer(), _movH265info.pFile, 0/*MOV_FLAG_FASTSTART*/);

	_strFileTmp = strFileTmp;
	_strFile = strFile;
	_ticker.resetTime();

	if ( _h265Record ){
		auto videoTrack = dynamic_pointer_cast<H265Track>(getTrack(TrackVideo));
		if(videoTrack){
			_movH265info.width = videoTrack->getVideoWidth();
			_movH265info.height = videoTrack->getVideoHeight();
		}
	}else{
		auto videoTrack = dynamic_pointer_cast<H264Track>(getTrack(TrackVideo));
		if(videoTrack){
//			auto &sps = videoTrack->getSps();
//			auto &pps = videoTrack->getPps();
			_movH265info.width = videoTrack->getVideoWidth();
			_movH265info.height = videoTrack->getVideoHeight();
		}		
	}
	if ( _movH265info.width <=0 || _movH265info.height <= 0 )
		WarnL << "分辨率获取失败,MP4录制异常";

	auto audioTrack = dynamic_pointer_cast<AACTrack>(getTrack(TrackAudio));
	if(audioTrack){
		_audioSampleRate = audioTrack->getAudioSampleRate();
		_audioChannel = audioTrack->getAudioChannel();
		uint8_t extra_data[64 * 1024];		
		_movH265info.audioTrack = mov_writer_add_audio(_movH265info.pMov, MOV_OBJECT_AAC, _audioChannel, 16, _audioSampleRate, audioTrack->getAacCfg().data(), 2);
		if (-1 == _movH265info.audioTrack) 
			WarnL << "添加音频通道失败:" << strFileTmp;
	}
}

void Mp4Maker::asyncClose() {
	auto strFileTmp = _strFileTmp;
	auto strFile = _strFile;
	auto info = _info;
	
	FILE *pFile = _movH265info.pFile;
	mov_writer_t*  hMp4 = _movH265info.pMov;
	WorkThreadPool::Instance().getExecutor()->async([hMp4,strFileTmp,strFile,info,pFile]() {
		//获取文件录制时间，放在MP4Close之前是为了忽略MP4Close执行时间
		const_cast<Mp4Info&>(info).ui64TimeLen = ::time(NULL) - info.ui64StartedTime;
		//MP4Close非常耗时，所以要放在后台线程执行
		if (hMp4 != NULL ){
			mov_writer_destroy(hMp4);			
			fclose(pFile);
			DebugL << "fclose end";
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
	if (_movH265info.pMov != NULL) {	
		asyncClose();
		memset(&_movH265info, 0, sizeof(_movH265info));
		_movH265info.pMov = NULL;
		_movH265info.videoTrack = -1;	
		_movH265info.audioTrack = -1;
	}	
}

void Mp4Maker::onTrackFrame(const Frame::Ptr &frame) {
	switch (frame->getCodecId()){
		case CodecH264:{
			//需要带00 00 00 01,方便在mov_writer_write的时候字节修改这4Byte为长度信息
			inputH264(frame->data() , frame->size(),frame->pts());
		}
			break;
		case CodecAAC:{
			//不需要ADTS头
			inputAAC(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize(),frame->pts());
		}
			break;
		case CodecH265:{
			//需要带00 00 00 01,方便在mov_writer_write的时候字节修改这4Byte为长度信息
			inputH265(frame->data() , frame->size(),frame->pts());
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


#endif //ENABLE_MP4RECORD