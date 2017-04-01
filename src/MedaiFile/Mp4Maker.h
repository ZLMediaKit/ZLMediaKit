/*
 * Mp4Maker.h
 *
 *  Created on: 2013-9-18
 *      Author: root
 */

#ifndef MP4MAKER_H_
#define MP4MAKER_H_

#ifdef ENABLE_MEDIAFILE

#include <mutex>
#include "Player/PlayerBase.h"
#include <memory>
#include "Util/logger.h"
#include "Util/util.h"
#include "Util/TimeTicker.h"
#include <mp4v2/mp4v2.h>
#include "Util/TimeTicker.h"

using namespace std;
using namespace ZL::Player;
using namespace ZL::Util;

namespace ZL {
namespace MediaFile {

class Mp4Info
{
public:
	time_t ui64StartedTime; //GMT标准时间，单位秒
	time_t ui64TimeLen;//录像长度，单位秒
	__off_t ui64FileSize;//文件大小，单位BYTE
	string strFilePath;//文件路径
	string strFileName;//文件名称
	string strFolder;//文件夹路径
	string strUrl;//播放路径
	string strAppName;//应用名称
	string strStreamId;//流ID
};
class Mp4Maker {
public:
	typedef std::shared_ptr<Mp4Maker> Ptr;
	Mp4Maker(const string &strPath,const string &strApp,const string &strStreamId, const PlayerBase::Ptr &pPlayer);
	virtual ~Mp4Maker();
	//时间戳：参考频率1000
	void inputH264(void *pData, uint32_t ui32Length, uint32_t ui32TimeStamp, int iType);
	//时间戳：参考频率1000
	void inputAAC(void *pData, uint32_t ui32Length, uint32_t ui32TimeStamp);
private:
	MP4FileHandle m_hMp4 = MP4_INVALID_FILE_HANDLE;
	MP4TrackId m_hVideo = MP4_INVALID_TRACK_ID;
	MP4TrackId m_hAudio = MP4_INVALID_TRACK_ID;
	PlayerBase::Ptr m_pPlayer;
	string m_strPath;
	string m_strFile;
	string m_strFileTmp;
	Ticker m_ticker;
	SmoothTicker m_mediaTicker[2];

	void createFile();
	void closeFile();
	void _inputH264(void *pData, uint32_t ui32Length, uint32_t ui64Duration, int iType);
	void _inputAAC(void *pData, uint32_t ui32Length, uint32_t ui64Duration);

	string m_strLastVideo;
	string m_strLastAudio;

	uint32_t m_ui32LastVideoTime = 0;
	uint32_t m_ui32LastAudioTime = 0;
	int m_iLastVideoType = 0;

	Mp4Info m_info;
};

} /* namespace MediaFile */
} /* namespace ZL */

#endif ///ENABLE_MEDIAFILE

#endif /* MP4MAKER_H_ */
