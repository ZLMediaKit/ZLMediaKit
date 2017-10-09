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

#include "MediaRecorder.h"
#include "Common/config.h"
#include "Http/HttpSession.h"
#include "Util/util.h"
#include "Util/mini.h"
#include "Network/sockutil.h"

using namespace ZL::Util;
using namespace ZL::Network;

namespace ZL {
namespace MediaFile {

MediaRecorder::MediaRecorder(const string &strApp,const string &strId,const std::shared_ptr<PlayerBase> &pPlayer) {
#ifdef ENABLE_HLS
	static string hlsPrefix = mINI::Instance()[Config::Http::kHttpPrefix];
	static string hlsPath = mINI::Instance()[Config::Hls::kFilePath];
	static uint32_t hlsBufSize = mINI::Instance()[Config::Hls::kFileBufSize].as<uint32_t>();
	static uint32_t hlsDuration = mINI::Instance()[Config::Hls::kSegmentDuration].as<uint32_t>();
	static uint32_t hlsNum = mINI::Instance()[Config::Hls::kSegmentNum].as<uint32_t>();

	m_hlsMaker.reset(new HLSMaker(hlsPath + "/" + strApp + "/" + strId + "/hls.m3u8",
										hlsPrefix + "/" + strApp + "/" + strId + "/",
										hlsBufSize,hlsDuration,hlsNum));
#endif //ENABLE_HLS

#ifdef ENABLE_MP4V2
	static string recordPath = mINI::Instance()[Config::Record::kFilePath];
	static string recordAppName = mINI::Instance()[Config::Record::kAppName];
	m_mp4Maker.reset(new Mp4Maker(recordPath + "/" + recordAppName + "/" + strApp + "/"  + strId + "/",
									strApp,strId,pPlayer));
#endif //ENABLE_MP4V2
}

MediaRecorder::~MediaRecorder() {
}

void MediaRecorder::inputH264(void* pData, uint32_t ui32Length, uint32_t ui32TimeStamp, int iType) {
#ifdef ENABLE_HLS
	m_hlsMaker->inputH264(pData, ui32Length, ui32TimeStamp * 90, iType);
#endif //ENABLE_HLS

#ifdef ENABLE_MP4V2
	m_mp4Maker->inputH264(pData, ui32Length, ui32TimeStamp, iType);
#endif //ENABLE_MP4V2
}

void MediaRecorder::inputAAC(void* pData, uint32_t ui32Length, uint32_t ui32TimeStamp) {
#ifdef ENABLE_HLS
	m_hlsMaker->inputAAC(pData, ui32Length, ui32TimeStamp * 90);
#endif //ENABLE_HLS

#ifdef ENABLE_MP4V2
	m_mp4Maker->inputAAC(pData, ui32Length, ui32TimeStamp);
#endif //ENABLE_MP4V2
}

} /* namespace MediaFile */
} /* namespace ZL */
