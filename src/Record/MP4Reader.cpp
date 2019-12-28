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

#include "MP4Reader.h"
#include "Common/config.h"
#include "Util/mini.h"
#include "Util/File.h"
#include "Http/HttpSession.h"
#include "Extension/AAC.h"
#include "Extension/H264.h"
#include "Thread/WorkThreadPool.h"

using namespace toolkit;

namespace mediakit {

#ifdef ENABLE_MP4V2
MP4Reader::MP4Reader(const string &strVhost,const string &strApp, const string &strId,const string &filePath ) {
	_poller = WorkThreadPool::Instance().getPoller();
    auto strFileName = filePath;
    if(strFileName.empty()){
		GET_CONFIG(string,recordPath,Record::kFilePath);
        GET_CONFIG(bool,enableVhost,General::kEnableVhost);
        if(enableVhost){
            strFileName = strVhost + "/" + strApp + "/" + strId;
        }else{
            strFileName = strApp + "/" + strId;
        }
		strFileName = File::absolutePath(strFileName,recordPath);
    }

	_hMP4File = MP4Read(strFileName.data());
	if(_hMP4File == MP4_INVALID_FILE_HANDLE){
		throw runtime_error(StrPrinter << "打开MP4文件失败:" << strFileName << endl);
	}
	_video_trId = MP4FindTrackId(_hMP4File, 0, MP4_VIDEO_TRACK_TYPE, 0);
	if(_video_trId != MP4_INVALID_TRACK_ID){
		 if(strcmp(MP4GetTrackMediaDataName(_hMP4File, _video_trId),"avc1") ==0){
			auto _video_timescale 		= MP4GetTrackTimeScale(_hMP4File, _video_trId);
			auto _video_duration 		= MP4GetTrackDuration(_hMP4File, _video_trId);
			_video_num_samples     = MP4GetTrackNumberOfSamples(_hMP4File, _video_trId);
			_video_sample_max_size = MP4GetTrackMaxSampleSize(_hMP4File, _video_trId);
			_video_width 			= MP4GetTrackVideoWidth(_hMP4File, _video_trId);
			_video_height 			= MP4GetTrackVideoHeight(_hMP4File, _video_trId);
			_video_framerate       = MP4GetTrackVideoFrameRate(_hMP4File, _video_trId);
			_pcVideoSample = std::shared_ptr<uint8_t> (new uint8_t[_video_sample_max_size],[](uint8_t *ptr){
				delete [] ptr;
			});
			uint8_t **seqheader;
			uint8_t **pictheader;
			uint32_t *pictheadersize;
			uint32_t *seqheadersize;
			uint32_t ix;
			if(MP4GetTrackH264SeqPictHeaders(_hMP4File, _video_trId, &seqheader, &seqheadersize, &pictheader, &pictheadersize)){
				for (ix = 0; seqheadersize[ix] != 0; ix++) {
					_strSps.assign((char *)(seqheader[ix]), seqheadersize[ix]);
					float framerate;
					getAVCInfo(_strSps, (int &)_video_width, (int &)_video_height, framerate);
					_video_framerate = framerate;
					_strSps = string("\x0\x0\x0\x1",4) + _strSps;
					MP4Free(seqheader[ix]);
				}
				MP4Free(seqheader);
				MP4Free(seqheadersize);
				for (ix = 0; pictheadersize[ix] != 0; ix++) {
					_strPps.assign("\x0\x0\x0\x1",4);
					_strPps.append((char *)(pictheader[ix]), pictheadersize[ix]);
					MP4Free(pictheader[ix]);
				}
				MP4Free(pictheader);
				MP4Free(pictheadersize);
			}
			_video_ms = 1000.0 * _video_duration / _video_timescale;
			/*InfoL 	<< "\r\n"
					<< _video_ms << "\r\n"
					<< _video_num_samples << "\r\n"
					<< _video_framerate << "\r\n"
					<< _video_width << "\r\n"
					<< _video_height << "\r\n";*/
		} else {
			//如果不是h264，则忽略
			_video_trId = MP4_INVALID_TRACK_ID;
		}
	}


	_audio_trId = MP4FindTrackId(_hMP4File, 0, MP4_AUDIO_TRACK_TYPE, 0);
	if (_audio_trId != MP4_INVALID_TRACK_ID) {
		if (strcmp(MP4GetTrackMediaDataName(_hMP4File, _audio_trId), "mp4a") == 0) {
			_audio_sample_rate = MP4GetTrackTimeScale(_hMP4File, _audio_trId);
			auto _audio_duration = MP4GetTrackDuration(_hMP4File, _audio_trId);
			_audio_num_samples = MP4GetTrackNumberOfSamples(_hMP4File,_audio_trId);
			_audio_num_channels = MP4GetTrackAudioChannels(_hMP4File, _audio_trId);
			_audio_sample_max_size = MP4GetTrackMaxSampleSize(_hMP4File,_audio_trId);
			uint8_t *ppConfig;
			uint32_t pConfigSize;
			if(MP4GetTrackESConfiguration(_hMP4File,_audio_trId,&ppConfig,&pConfigSize)){
				_strAacCfg.assign((char *)ppConfig, pConfigSize);
				makeAdtsHeader(_strAacCfg, _adts);
				writeAdtsHeader(_adts,_adts.buffer);
				getAACInfo(_adts, (int &)_audio_sample_rate, (int &)_audio_num_channels);
				MP4Free(ppConfig);
			}
			_audio_ms = 1000.0 * _audio_duration / _audio_sample_rate;
			/*InfoL 	<< "\r\n"
					<< _audio_ms << "\r\n"
					<< _audio_num_samples << "\r\n"
					<< _audio_num_channels << "\r\n"
					<< _audio_sample_rate << "\r\n";*/
		}else{
			_audio_trId = MP4_INVALID_TRACK_ID;
		}
	}
	if(_audio_trId == MP4_INVALID_TRACK_ID && _video_trId == MP4_INVALID_TRACK_ID){
		MP4Close(_hMP4File);
		_hMP4File = MP4_INVALID_FILE_HANDLE;
		throw runtime_error(StrPrinter << "该MP4文件音视频格式不支持:" << strFileName << endl);
	}

	_iDuration	= MAX(_video_ms,_audio_ms);
	_mediaMuxer.reset(new MultiMediaSourceMuxer(strVhost, strApp, strId, _iDuration / 1000.0, true, true, false, false));
	if (_audio_trId != MP4_INVALID_TRACK_ID) {
		AACTrack::Ptr track = std::make_shared<AACTrack>(_strAacCfg);
		_mediaMuxer->addTrack(track);
	}

	if (_video_trId != MP4_INVALID_TRACK_ID) {
		H264Track::Ptr track = std::make_shared<H264Track>(_strSps,_strPps);
		_mediaMuxer->addTrack(track);
	}
}


MP4Reader::~MP4Reader() {
	if (_hMP4File != MP4_INVALID_FILE_HANDLE) {
		MP4Close(_hMP4File);
		_hMP4File = MP4_INVALID_FILE_HANDLE;
	}
}


void MP4Reader::startReadMP4() {
	auto strongSelf = shared_from_this();
    GET_CONFIG(uint32_t,sampleMS,Record::kSampleMS);

	_timer = std::make_shared<Timer>(sampleMS / 1000.0f,[strongSelf](){
		return strongSelf->readSample(0,false);
	}, _poller);

    //先读sampleMS毫秒的数据用于产生MediaSouce
	readSample(sampleMS, false);
	_mediaMuxer->setListener(strongSelf);
}
 bool MP4Reader::seekTo(MediaSource &sender,uint32_t ui32Stamp){
	 seek(ui32Stamp);
	 return true;
}
bool MP4Reader::close(MediaSource &sender,bool force){
    if(!_mediaMuxer || (!force && _mediaMuxer->totalReaderCount())){
        return false;
    }
	_timer.reset();
    WarnL << sender.getSchema() << "/" << sender.getVhost() << "/" << sender.getApp() << "/" << sender.getId() << " " << force;
    return true;
}

void MP4Reader::onNoneReader(MediaSource &sender) {
    if(!_mediaMuxer || _mediaMuxer->totalReaderCount()){
        return;
    }
    MediaSourceEvent::onNoneReader(sender);
}

int MP4Reader::totalReaderCount(MediaSource &sender) {
	return _mediaMuxer ? _mediaMuxer->totalReaderCount() : sender.readerCount();
}

bool MP4Reader::readSample(int iTimeInc,bool justSeekSyncFrame) {
	TimeTicker();
	lock_guard<recursive_mutex> lck(_mtx);
	auto bFlag0 = readVideoSample(iTimeInc,justSeekSyncFrame);//数据没读完
	auto bFlag1 = readAudioSample(iTimeInc,justSeekSyncFrame);//数据没读完
	auto bFlag2 = _mediaMuxer->totalReaderCount() > 0;//读取者大于0
	if((bFlag0 || bFlag1) && bFlag2){
		_alive.resetTime();
	}
	//重头开始循环读取
	GET_CONFIG(bool,fileRepeat,Record::kFileRepeat);
	if (fileRepeat && !bFlag0 && !bFlag1) {
		seek(0);
	}
	//DebugL << "alive ...";
	//3秒延时关闭
	return  _alive.elapsedTime() <  3 * 1000;
}
inline bool MP4Reader::readVideoSample(int iTimeInc,bool justSeekSyncFrame) {
	if (_video_trId != MP4_INVALID_TRACK_ID) {
		auto iNextSample = getVideoSampleId(iTimeInc);
		MP4SampleId iIdx = _video_current;
		for (; iIdx < iNextSample; iIdx++) {
			uint8_t *pBytes = _pcVideoSample.get();
			uint32_t numBytes = _video_sample_max_size;
			MP4Duration pRenderingOffset;
			if(MP4ReadSample(_hMP4File, _video_trId, iIdx + 1, &pBytes, &numBytes,NULL,NULL,&pRenderingOffset,&_bSyncSample)){
				if (!justSeekSyncFrame) {
					uint32_t iOffset = 0;
					while (iOffset < numBytes) {
						uint32_t iFrameLen;
						memcpy(&iFrameLen,pBytes + iOffset,4);
						iFrameLen = ntohl(iFrameLen);
                        if(iFrameLen + iOffset + 4> numBytes){
                            break;
                        }
						memcpy(pBytes + iOffset, "\x0\x0\x0\x1", 4);
						uint32_t dts = (double) _video_ms * iIdx / _video_num_samples;
						writeH264(pBytes + iOffset, iFrameLen + 4, dts, dts + pRenderingOffset / 90);
						iOffset += (iFrameLen + 4);
					}
				}else if(_bSyncSample){
					break;
				}
			}else{
				ErrorL << "读取视频失败:" << iIdx + 1;
			}
		}
		_video_current = iIdx;
		return _video_current < _video_num_samples;
	}
	return false;
}

inline bool MP4Reader::readAudioSample(int iTimeInc,bool justSeekSyncFrame) {
	if (_audio_trId != MP4_INVALID_TRACK_ID) {
		auto iNextSample = getAudioSampleId(iTimeInc);
		for (auto i = _audio_current; i < iNextSample; i++) {
			uint32_t numBytes = _audio_sample_max_size;
			uint8_t *pBytes = _adts.buffer + 7;
			if(MP4ReadSample(_hMP4File, _audio_trId, i + 1, &pBytes, &numBytes)){
				if (!justSeekSyncFrame) {
					_adts.aac_frame_length = 7 + numBytes;
					writeAdtsHeader(_adts, _adts.buffer);
					writeAAC(_adts.buffer, _adts.aac_frame_length, (double) _audio_ms * i / _audio_num_samples);
				}
			}else{
				ErrorL << "读取音频失败:" << i+ 1;
			}
		}
		_audio_current = iNextSample;
		return _audio_current < _audio_num_samples;
	}
	return false;
}

inline void MP4Reader::writeH264(uint8_t *pucData,int iLen,uint32_t dts,uint32_t pts) {
	_mediaMuxer->inputFrame(std::make_shared<H264FrameNoCacheAble>((char*)pucData,iLen,dts,pts));
}

inline void MP4Reader::writeAAC(uint8_t *pucData,int iLen,uint32_t uiStamp) {
	_mediaMuxer->inputFrame(std::make_shared<AACFrameNoCacheAble>((char*)pucData,iLen,uiStamp));
}

inline MP4SampleId MP4Reader::getVideoSampleId(int iTimeInc ) {
	MP4SampleId video_current = (double)_video_num_samples *  (_iSeekTime + _ticker.elapsedTime() + iTimeInc) / _video_ms;
	video_current = MAX(0,MIN(_video_num_samples, video_current));
	return video_current;

}

inline MP4SampleId MP4Reader::getAudioSampleId(int iTimeInc) {
	MP4SampleId audio_current = (double)_audio_num_samples * (_iSeekTime + _ticker.elapsedTime() + iTimeInc)  / _audio_ms ;
	audio_current = MAX(0,MIN(_audio_num_samples,audio_current));
	return audio_current;
}
inline void MP4Reader::setSeekTime(uint32_t iSeekTime){
	_iSeekTime = MAX(0, MIN(iSeekTime,_iDuration));
	_ticker.resetTime();
	if (_audio_trId != MP4_INVALID_TRACK_ID) {
		_audio_current = getAudioSampleId();
	}
	if (_video_trId != MP4_INVALID_TRACK_ID) {
		_video_current = getVideoSampleId();
	}
}

inline uint32_t MP4Reader::getVideoCurrentTime(){
	return (double)_video_current * _video_ms /_video_num_samples;
}
void MP4Reader::seek(uint32_t iSeekTime,bool bReStart){
	lock_guard<recursive_mutex> lck(_mtx);
	if(iSeekTime == 0 || _video_trId == MP4_INVALID_TRACK_ID){
		setSeekTime(iSeekTime);
	}else{
		setSeekTime(iSeekTime - 5000);
		//在之后的10秒查找关键帧
		readVideoSample(10000, true);
		if (_bSyncSample) {
			//找到关键帧
			auto iIdr =  _video_current;
			setSeekTime(getVideoCurrentTime());
			_video_current = iIdr;
		}else{
			//未找到关键帧
			setSeekTime(iSeekTime);
		}
	}
	_mediaMuxer->setTimeStamp(_iSeekTime);

	if(bReStart){
		_timer.reset();
		startReadMP4();
	}
}

#endif //ENABLE_MP4V2



MediaSource::Ptr MP4Reader::onMakeMediaSource(const string &strSchema,
												const string &strVhost,
												const string &strApp,
												const string &strId,
												const string &filePath,
												bool checkApp ){
#ifdef ENABLE_MP4V2
    GET_CONFIG(string,appName,Record::kAppName);
    if (checkApp && strApp != appName) {
		return nullptr;
	}
	try {
		MP4Reader::Ptr pReader(new MP4Reader(strVhost,strApp, strId,filePath));
		pReader->startReadMP4();
		return MediaSource::find(strSchema,strVhost,strApp, strId, false);
	} catch (std::exception &ex) {
		WarnL << ex.what();
		return nullptr;
	}
#else
	return nullptr;
#endif //ENABLE_MP4V2
}



} /* namespace mediakit */
