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

#ifndef MP4MAKER_H_
#define MP4MAKER_H_

#ifdef ENABLE_MP4RECORD

#include <mutex>
#include <memory>
#include "Player/PlayerBase.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Util/TimeTicker.h"
#include "Common/MediaSink.h"
#include "Extension/Track.h"


#include "mov-writer.h"
#include "mpeg4-hevc.h"
#include "mpeg4-avc.h"


using namespace toolkit;

namespace mediakit {

class Mp4Info {
public:
	time_t ui64StartedTime; //GMT标准时间，单位秒
	time_t ui64TimeLen;//录像长度，单位秒
	off_t ui64FileSize;//文件大小，单位BYTE
	string strFilePath;//文件路径
	string strFileName;//文件名称
	string strFolder;//文件夹路径
	string strUrl;//播放路径
	string strAppName;//应用名称
	string strStreamId;//流ID
	string strVhost;//vhost
};

class MovH265Info {
public:
	mov_writer_t* pMov;
	struct mpeg4_hevc_t hevc;
	struct mpeg4_avc_t avc;
	int videoTrack;
	int audioTrack;
	int width;
	int height;
	uint32_t startPts;
	FILE * pFile;
};

class Mp4Maker : public MediaSink{
public:
	typedef std::shared_ptr<Mp4Maker> Ptr;
	Mp4Maker(const string &strPath,
			 const string &strVhost ,
			 const string &strApp,
			 const string &strStreamId);
	virtual ~Mp4Maker();
private:
	/**
     * 某Track输出frame，在onAllTrackReady触发后才会调用此方法
     * @param frame
     */
	void onTrackFrame(const Frame::Ptr &frame) override ;

	/**
	 * 所有Track准备好了
	 */
	void onAllTrackReady() override;
private:
    void createFile();
    void closeFile();
    void asyncClose();

	//时间戳：参考频率1000
	void inputH264(void *pData, uint32_t ui32Length, uint32_t ui32TimeStamp);	
	void inputH265(void *pData, uint32_t ui32Length, uint32_t ui32TimeStamp);
	//时间戳：参考频率1000
	void inputAAC(void *pData, uint32_t ui32Length, uint32_t ui32TimeStamp);

	void inputH264_l(void *pData, uint32_t ui32Length, uint32_t ui32TimeStamp);	
	void inputH265_l(void *pData, uint32_t ui32Length, uint32_t ui32TimeStamp);
    void inputAAC_l(void *pData, uint32_t ui32Length, uint32_t ui32TimeStamp);
private:

	MovH265Info _movH265info;
	int _h265Record = 0;
	uint32_t _startPts;
	
	uint8_t _sBbuffer[2 * 1024 * 1024];
	string _strPath;
	string _strFile;
	string _strFileTmp;
	Ticker _ticker;

	string _strLastVideo;
	string _strLastAudio;

	uint32_t _ui32LastVideoTime = 0;
	uint32_t _ui32LastAudioTime = 0;
	Mp4Info _info;

	bool _haveVideo = false;
	int _audioSampleRate;
	int _audioChannel;
};

} /* namespace mediakit */

#endif ///ENABLE_MP4RECORD

#endif /* MP4MAKER_H_ */
