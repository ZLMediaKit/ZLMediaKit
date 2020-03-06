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

#ifndef SRC_MEDIAFILE_MEDIAREADER_H_
#define SRC_MEDIAFILE_MEDIAREADER_H_

#include "Common/MultiMediaSourceMuxer.h"
#include "Extension/AAC.h"

#ifdef ENABLE_MP4V2
#include <mp4v2/mp4v2.h>
#endif //ENABLE_MP4V2
using namespace toolkit;

namespace mediakit {

class MP4Reader : public std::enable_shared_from_this<MP4Reader> ,public MediaSourceEvent{
public:
	typedef std::shared_ptr<MP4Reader> Ptr;
	virtual ~MP4Reader();

	/**
	 * 流化一个mp4文件，使之转换成RtspMediaSource和RtmpMediaSource
	 * @param strVhost 虚拟主机
	 * @param strApp 应用名
	 * @param strId 流id
	 * @param filePath 文件路径，如果为空则根据配置文件和上面参数自动生成，否则使用指定的文件
	 */
	MP4Reader(const string &strVhost,const string &strApp, const string &strId,const string &filePath = "");
	/**
	 * 开始流化MP4文件，需要指出的是，MP4Reader对象一经过调用startReadMP4方法，它的强引用会自持有，
	 * 意思是在文件流化结束之前或中断之前,MP4Reader对象是不会被销毁的(不管有没有被外部对象持有)
	 */
	void startReadMP4();

	/**
	 * 自动生成MP4Reader对象然后查找相关的MediaSource对象
	 * @param strSchema 协议名
 	 * @param strVhost 虚拟主机
	 * @param strApp 应用名
	 * @param strId 流id
	 * @param filePath 文件路径，如果为空则根据配置文件和上面参数自动生成，否则使用指定的文件
	 * @param checkApp 是否检查app，防止服务器上文件被乱访问
	 * @return MediaSource
	 */
	static MediaSource::Ptr onMakeMediaSource(const string &strSchema,
											  const string &strVhost,
											  const string &strApp,
											  const string &strId,
											  const string &filePath = "",
											  bool checkApp = true);

private:
	//MediaSourceEvent override
	bool seekTo(MediaSource &sender,uint32_t ui32Stamp) override;
	bool close(MediaSource &sender,bool force) override;
    void onNoneReader(MediaSource &sender) override;
	int totalReaderCount(MediaSource &sender) override;
#ifdef ENABLE_MP4V2
    void seek(uint32_t iSeekTime,bool bReStart = true);
	inline void setSeekTime(uint32_t iSeekTime);
	inline uint32_t getVideoCurrentTime();
	inline MP4SampleId getVideoSampleId(int iTimeInc = 0);
	inline MP4SampleId getAudioSampleId(int iTimeInc = 0);
	bool readSample(int iTimeInc, bool justSeekSyncFrame);
	inline bool readVideoSample(int iTimeInc,bool justSeekSyncFrame);
	inline bool readAudioSample(int iTimeInc,bool justSeekSyncFrame);
	inline void writeH264(uint8_t *pucData,int iLen,uint32_t dts,uint32_t pts);
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
	EventPoller::Ptr _poller;
#endif //ENABLE_MP4V2
};

} /* namespace mediakit */

#endif /* SRC_MEDIAFILE_MEDIAREADER_H_ */
