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

#ifndef SRC_MEDIAFILE_MEDIAREADER_H_
#define SRC_MEDIAFILE_MEDIAREADER_H_

#include "Common/Device.h"
#include "Rtsp/RtspMediaSource.h"
#include "Rtmp/RtmpMediaSource.h"

#ifdef ENABLE_MP4V2
#include <mp4v2/mp4v2.h>
#endif //ENABLE_MP4V2
using namespace toolkit;

namespace mediakit {

class MediaReader : public std::enable_shared_from_this<MediaReader> ,public MediaSourceEvent{
public:
	typedef std::shared_ptr<MediaReader> Ptr;
	MediaReader(const string &strVhost,const string &strApp, const string &strId);
	virtual ~MediaReader();
	static MediaSource::Ptr onMakeMediaSource(const string &strSchema,const string &strVhost,const string &strApp, const string &strId);
public:
	bool seekTo(uint32_t ui32Stamp) override;
    bool close() override;

#ifdef ENABLE_MP4V2
private:
	void seek(uint32_t iSeekTime,bool bReStart = true);
	inline void setSeekTime(uint32_t iSeekTime);
	inline uint32_t getVideoCurrentTime();
	void startReadMP4();
	inline MP4SampleId getVideoSampleId(int iTimeInc = 0);
	inline MP4SampleId getAudioSampleId(int iTimeInc = 0);
	bool readSample(int iTimeInc, bool justSeekSyncFrame);
	inline bool readVideoSample(int iTimeInc,bool justSeekSyncFrame);
	inline bool readAudioSample(int iTimeInc,bool justSeekSyncFrame);
	inline void writeH264(uint8_t *pucData,int iLen,uint32_t uiStamp);
	inline void writeAAC(uint8_t *pucData,int iLen,uint32_t uiStamp);
private:
	MP4FileHandle _hMP4File = MP4_INVALID_FILE_HANDLE;
	MP4TrackId _video_trId = MP4_INVALID_TRACK_ID;
	uint32_t _video_ms = 0;
	uint32_t _video_num_samples = 0;
	uint32_t _video_sample_max_size = 0;
	uint32_t _video_width = 0;
	uint32_t _video_height = 0;
	uint32_t _video_framerate = 0;
	string _strPps;
	string _strSps;
	bool _bSyncSample  = false;

	MP4TrackId _audio_trId = MP4_INVALID_TRACK_ID;
	uint32_t _audio_ms = 0;
	uint32_t _audio_num_samples = 0;
	uint32_t _audio_sample_max_size = 0;
	uint32_t _audio_sample_rate = 0;
	uint32_t _audio_num_channels = 0;
	string _strAacCfg;
	AACFrame _adts;

	int _iDuration = 0;
	MultiMediaSourceMuxer::Ptr _mediaMuxer;
	MP4SampleId _video_current = 0;
	MP4SampleId _audio_current = 0;
	std::shared_ptr<uint8_t> _pcVideoSample;

	int _iSeekTime = 0 ;
	Ticker _ticker;
	Ticker _alive;
	recursive_mutex _mtx;
	Timer::Ptr _timer;
#endif //ENABLE_MP4V2
};

} /* namespace mediakit */

#endif /* SRC_MEDIAFILE_MEDIAREADER_H_ */
