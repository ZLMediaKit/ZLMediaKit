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

MediaRecorder::MediaRecorder(const string &strVhost ,const string &strApp,const string &strId,const std::shared_ptr<PlayerBase> &pPlayer) {
	static string hlsPrefix = mINI::Instance()[Config::Hls::kHttpPrefix];
	static string hlsPrefixDefaultVhost = mINI::Instance()[Config::Hls::kHttpPrefixDefaultVhost];
	static string hlsPath = mINI::Instance()[Config::Hls::kFilePath];
	static uint32_t hlsBufSize = mINI::Instance()[Config::Hls::kFileBufSize].as<uint32_t>();
	static uint32_t hlsDuration = mINI::Instance()[Config::Hls::kSegmentDuration].as<uint32_t>();
	static uint32_t hlsNum = mINI::Instance()[Config::Hls::kSegmentNum].as<uint32_t>();

	string hlsPrefixVhost = hlsPrefix;
	do{
		//生成hls http前缀
		if (strVhost.empty() || strVhost == DEFAULT_VHOST) {
			hlsPrefixVhost = hlsPrefixDefaultVhost;
			break;
		}
		auto pos_start = hlsPrefixVhost.find("${");
		auto pos_end = hlsPrefixVhost.find("}");
		if (pos_start != string::npos && pos_end != string::npos && pos_end - pos_start - 2 > 0 ) {
			auto key = hlsPrefixVhost.substr(pos_start + 2, pos_end - pos_start - 2);
			trim(key);
			if (key == VHOST_KEY) {
				hlsPrefixVhost.replace(pos_start, pos_end - pos_start + 1, strVhost);
			}
		}
	}while(0);
	m_hlsMaker.reset(new HLSMaker(hlsPath + "/" + strVhost + "/" + strApp + "/" + strId + "/hls.m3u8",
								  hlsPrefixVhost + "/" + strApp + "/" + strId + "/",
								  hlsBufSize,hlsDuration,hlsNum));
#ifdef ENABLE_MP4V2
	static string recordPath = mINI::Instance()[Config::Record::kFilePath];
	static string recordAppName = mINI::Instance()[Config::Record::kAppName];
	m_mp4Maker.reset(new Mp4Maker(recordPath + "/" + strVhost + "/" + recordAppName + "/" + strApp + "/"  + strId + "/",
								  strVhost,strApp,strId,pPlayer));
#endif //ENABLE_MP4V2
}

MediaRecorder::~MediaRecorder() {
}

void MediaRecorder::inputH264(void* pData, uint32_t ui32Length, uint32_t ui32TimeStamp, int iType) {
	m_hlsMaker->inputH264(pData, ui32Length, ui32TimeStamp * 90, iType);
#ifdef ENABLE_MP4V2
	m_mp4Maker->inputH264(pData, ui32Length, ui32TimeStamp, iType);
#endif //ENABLE_MP4V2
}

void MediaRecorder::inputAAC(void* pData, uint32_t ui32Length, uint32_t ui32TimeStamp) {
	m_hlsMaker->inputAAC(pData, ui32Length, ui32TimeStamp * 90);
#ifdef ENABLE_MP4V2
	m_mp4Maker->inputAAC(pData, ui32Length, ui32TimeStamp);
#endif //ENABLE_MP4V2
}

} /* namespace MediaFile */
} /* namespace ZL */
