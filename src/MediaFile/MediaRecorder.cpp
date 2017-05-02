/*
 * MediaRecorder.cpp
 *
 *  Created on: 2016年12月8日
 *      Author: xzl
 */

#include "MediaRecorder.h"
#include "config.h"
#include "Http/HttpSession.h"
#include "Util/util.h"
#include "Util/mini.h"
#include "Network/sockutil.h"

using namespace ZL::Util;
using namespace ZL::Network;

namespace ZL {
namespace MediaFile {

MediaRecorder::MediaRecorder(const string &strApp,const string &strId,const std::shared_ptr<PlayerBase> &pPlayer) {
#ifdef ENABLE_MEDIAFILE
	static string hlsPrefix = mINI::Instance()[Config::Http::kHttpPrefix];
	static string hlsPath = mINI::Instance()[Config::Hls::kFilePath];
	static uint32_t hlsBufSize = mINI::Instance()[Config::Hls::kFileBufSize].as<uint32_t>();
	static uint32_t hlsDuration = mINI::Instance()[Config::Hls::kSegmentDuration].as<uint32_t>();
	static uint32_t hlsNum = mINI::Instance()[Config::Hls::kSegmentNum].as<uint32_t>();

	static string recordPath = mINI::Instance()[Config::Record::kFilePath];
	static string recordAppName = mINI::Instance()[Config::Record::kAppName];


	m_hlsMaker.reset(new HLSMaker(hlsPath + "/" + strApp + "/" + strId + "/hls.m3u8",
									hlsPrefix + "/" + strApp + "/" + strId + "/",
									hlsBufSize,hlsDuration,hlsNum));

	m_mp4Maker.reset(new Mp4Maker(recordPath + "/" + recordAppName + "/" + strApp + "/"  + strId + "/",
									strApp,strId,pPlayer));
#endif //ENABLE_MEDIAFILE
}

MediaRecorder::~MediaRecorder() {
}

void MediaRecorder::inputH264(void* pData, uint32_t ui32Length, uint32_t ui32TimeStamp, int iType) {
#ifdef ENABLE_MEDIAFILE
	m_hlsMaker->inputH264(pData, ui32Length, ui32TimeStamp * 90, iType);
	m_mp4Maker->inputH264(pData, ui32Length, ui32TimeStamp, iType);

#endif //ENABLE_MEDIAFILE
}

void MediaRecorder::inputAAC(void* pData, uint32_t ui32Length, uint32_t ui32TimeStamp) {
#ifdef ENABLE_MEDIAFILE
	m_hlsMaker->inputAAC(pData, ui32Length, ui32TimeStamp * 90);
	m_mp4Maker->inputAAC(pData, ui32Length, ui32TimeStamp);
#endif //ENABLE_MEDIAFILE
}

} /* namespace MediaFile */
} /* namespace ZL */
