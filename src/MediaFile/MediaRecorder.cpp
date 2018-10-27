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
using namespace toolkit;

namespace mediakit {

MediaRecorder::MediaRecorder(const string &strVhost_tmp,
                             const string &strApp,
                             const string &strId,
                             bool enableHls,
                             bool enableMp4) {

    GET_CONFIG_AND_REGISTER(string,hlsPath,Hls::kFilePath);
    GET_CONFIG_AND_REGISTER(uint32_t,hlsBufSize,Hls::kFileBufSize);
    GET_CONFIG_AND_REGISTER(uint32_t,hlsDuration,Hls::kSegmentDuration);
    GET_CONFIG_AND_REGISTER(uint32_t,hlsNum,Hls::kSegmentNum);

    string strVhost = strVhost_tmp;
    if(trim(strVhost).empty()){
        //如果strVhost为空，则强制为默认虚拟主机
        strVhost = DEFAULT_VHOST;
    }

    if(enableHls) {
        auto m3u8FilePath = hlsPath + "/" + strVhost + "/" + strApp + "/" + strId + "/hls.m3u8";
        _hlsMaker.reset(new HLSMaker(m3u8FilePath,hlsBufSize, hlsDuration, hlsNum));
    }

#ifdef ENABLE_MP4V2
    GET_CONFIG_AND_REGISTER(string,recordPath,Record::kFilePath);
    GET_CONFIG_AND_REGISTER(string,recordAppName,Record::kAppName);

    if(enableMp4){
        auto mp4FilePath = recordPath + "/" + strVhost + "/" + recordAppName + "/" + strApp + "/"  + strId + "/";
        _mp4Maker.reset(new Mp4Maker(mp4FilePath,strVhost,strApp,strId));
    }
#endif //ENABLE_MP4V2
}

MediaRecorder::~MediaRecorder() {
}

void MediaRecorder::inputFrame(const Frame::Ptr &frame) {
    if (_hlsMaker) {
        _hlsMaker->inputFrame(frame);
    }
#ifdef ENABLE_MP4V2
    if (_mp4Maker) {
        _mp4Maker->inputFrame(frame);
    }
#endif //ENABLE_MP4V2
}

void MediaRecorder::addTrack(const Track::Ptr &track) {
    if (_hlsMaker) {
        _hlsMaker->addTrack(track);
    }
#ifdef ENABLE_MP4V2
    if (_mp4Maker) {
        _mp4Maker->addTrack(track);
    }
#endif //ENABLE_MP4V2
}

} /* namespace mediakit */
