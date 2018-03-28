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

MediaRecorder::MediaRecorder(const string &strVhost_tmp,
                             const string &strApp,
                             const string &strId,
                             const std::shared_ptr<PlayerBase> &pPlayer,
                             bool enableHls,
                             bool enableMp4) {

    GET_CONFIG_AND_REGISTER(string,hlsPath,Config::Hls::kFilePath);
    GET_CONFIG_AND_REGISTER(uint32_t,hlsBufSize,Config::Hls::kFileBufSize);
    GET_CONFIG_AND_REGISTER(uint32_t,hlsDuration,Config::Hls::kSegmentDuration);
    GET_CONFIG_AND_REGISTER(uint32_t,hlsNum,Config::Hls::kSegmentNum);

    string strVhost = strVhost_tmp;
    if(trim(strVhost).empty()){
        //如果strVhost为空，则强制为默认虚拟主机
        strVhost = DEFAULT_VHOST;
    }

    if(enableHls) {
        auto m3u8FilePath = hlsPath + "/" + strVhost + "/" + strApp + "/" + strId + "/hls.m3u8";
        m_hlsMaker.reset(new HLSMaker(m3u8FilePath,hlsBufSize, hlsDuration, hlsNum));
    }

#ifdef ENABLE_MP4V2
    GET_CONFIG_AND_REGISTER(string,recordPath,Config::Record::kFilePath);
    GET_CONFIG_AND_REGISTER(string,recordAppName,Config::Record::kAppName);

    if(enableMp4){
        auto mp4FilePath = recordPath + "/" + strVhost + "/" + recordAppName + "/" + strApp + "/"  + strId + "/";
        m_mp4Maker.reset(new Mp4Maker(mp4FilePath,strVhost,strApp,strId,pPlayer));
    }
#endif //ENABLE_MP4V2
}

MediaRecorder::~MediaRecorder() {
}

void MediaRecorder::inputH264(void* pData, uint32_t ui32Length, uint32_t ui32TimeStamp, int iType) {
    if(m_hlsMaker){
        m_hlsMaker->inputH264(pData, ui32Length, ui32TimeStamp, iType);
    }
#ifdef ENABLE_MP4V2
    if(m_mp4Maker){
        m_mp4Maker->inputH264(pData, ui32Length, ui32TimeStamp, iType);
    }
#endif //ENABLE_MP4V2
}

void MediaRecorder::inputAAC(void* pData, uint32_t ui32Length, uint32_t ui32TimeStamp) {
    if(m_hlsMaker){
        m_hlsMaker->inputAAC(pData, ui32Length, ui32TimeStamp);
    }
#ifdef ENABLE_MP4V2
    if(m_mp4Maker){
        m_mp4Maker->inputAAC(pData, ui32Length, ui32TimeStamp);
    }
#endif //ENABLE_MP4V2
}

} /* namespace MediaFile */
} /* namespace ZL */
