/*
 * MediaReader.h
 *
 *  Created on: 2016年12月14日
 *      Author: xzl
 */

#ifndef SRC_MEDAIFILE_MEDIAREADER_H_
#define SRC_MEDAIFILE_MEDIAREADER_H_

#include "Rtsp/RtspMediaSource.h"
#include "Rtmp/RtmpMediaSource.h"
#include "Device/Device.h"

#ifdef ENABLE_MEDIAFILE
#include <mp4v2/mp4v2.h>
#endif //ENABLE_MEDIAFILE

using namespace ZL::DEV;
using namespace ZL::Rtsp;
using namespace ZL::Rtmp;

namespace ZL {
namespace MediaFile {

class MediaReader : public std::enable_shared_from_this<MediaReader>{
public:
	typedef std::shared_ptr<MediaReader> Ptr;
	MediaReader(const string &strApp, const string &strId);
	virtual ~MediaReader();
	static RtspMediaSource::Ptr onMakeRtsp(const string &strApp, const string &strId);
	static RtmpMediaSource::Ptr onMakeRtmp(const string &strApp, const string &strId);
private:

#ifdef ENABLE_MEDIAFILE
	MP4FileHandle m_hMP4File = MP4_INVALID_FILE_HANDLE;
	MP4TrackId m_video_trId = MP4_INVALID_TRACK_ID;
	uint32_t m_video_ms = 0;
	uint32_t m_video_num_samples = 0;
	uint32_t m_video_sample_max_size = 0;
	uint32_t m_video_width = 0;
	uint32_t m_video_height = 0;
	uint32_t m_video_framerate = 0;
	string m_strPps;
	string m_strSps;
	bool m_bSyncSample  = false;

	MP4TrackId m_audio_trId = MP4_INVALID_TRACK_ID;
	uint32_t m_audio_ms = 0;
	uint32_t m_audio_num_samples = 0;
	uint32_t m_audio_sample_max_size = 0;
	uint32_t m_audio_sample_rate = 0;
	uint32_t m_audio_num_channels = 0;
	string m_strAacCfg;
	AdtsFrame m_adts;

	int m_iDuration = 0;
	DevChannel::Ptr m_pChn;
	MP4SampleId m_video_current = 0;
	MP4SampleId m_audio_current = 0;
	std::shared_ptr<uint8_t> m_pcVideoSample;

	int m_iSeekTime = 0 ;
	Ticker m_ticker;
	Ticker m_alive;
	recursive_mutex m_mtx;

	void seek(int iSeekTime,bool bReStart = true);
	inline void setSeekTime(int iSeekTime);
	inline uint32_t getVideoCurrentTime();
	void startReadMP4();
	inline MP4SampleId getVideoSampleId(int iTimeInc = 0);
	inline MP4SampleId getAudioSampleId(int iTimeInc = 0);
	bool readSample(int iTimeInc = 0);
	inline bool readVideoSample(int iTimeInc = 0);
	inline bool readAudioSample(int iTimeInc = 0);
	inline void writeH264(uint8_t *pucData,int iLen,uint32_t uiStamp);
	inline void writeAAC(uint8_t *pucData,int iLen,uint32_t uiStamp);
#endif
};

} /* namespace MediaFile */
} /* namespace ZL */

#endif /* SRC_MEDAIFILE_MEDIAREADER_H_ */
