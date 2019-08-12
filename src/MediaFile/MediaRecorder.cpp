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

#include "MediaRecorder.h"
#include "Common/config.h"
#include "Http/HttpSession.h"
#include "Util/util.h"
#include "Util/mini.h"
#include "Network/sockutil.h"
#include "HlsMakerImp.h"
using namespace toolkit;

namespace mediakit {

MediaRecorder::MediaRecorder(const string &strVhost_tmp,
                             const string &strApp,
                             const string &strId,
                             bool enableHls,
                             bool enableMp4) {

    GET_CONFIG(string,hlsPath,Hls::kFilePath);
    GET_CONFIG(uint32_t,hlsBufSize,Hls::kFileBufSize);
    GET_CONFIG(uint32_t,hlsDuration,Hls::kSegmentDuration);
    GET_CONFIG(uint32_t,hlsNum,Hls::kSegmentNum);
    GET_CONFIG(bool,enableVhost,General::kEnableVhost);

    string strVhost = strVhost_tmp;
    if(trim(strVhost).empty()){
        //如果strVhost为空，则强制为默认虚拟主机
        strVhost = DEFAULT_VHOST;
    }

#if defined(ENABLE_HLS)
    if(enableHls) {
        string m3u8FilePath;
        if(enableVhost){
            m3u8FilePath = hlsPath + "/" + strVhost + "/" + strApp + "/" + strId + "/hls.m3u8";
            _hlsMaker.reset(new HlsRecorder(m3u8FilePath,string(VHOST_KEY) + "=" + strVhost ,hlsBufSize, hlsDuration, hlsNum));
        }else{
            m3u8FilePath = hlsPath + "/" + strApp + "/" + strId + "/hls.m3u8";
            _hlsMaker.reset(new HlsRecorder(m3u8FilePath,"",hlsBufSize, hlsDuration, hlsNum));
        }
    }
#endif //defined(ENABLE_HLS)

#if defined(ENABLE_MP4V2)
    GET_CONFIG(string,recordPath,Record::kFilePath);
    GET_CONFIG(string,recordAppName,Record::kAppName);

    if(enableMp4){
        string mp4FilePath;
        if(enableVhost){
            mp4FilePath = recordPath + "/" + strVhost + "/" + recordAppName + "/" + strApp + "/"  + strId + "/";
        } else {
            mp4FilePath = recordPath + "/" + recordAppName + "/" + strApp + "/"  + strId + "/";
        }
        _mp4Maker.reset(new Mp4Maker(mp4FilePath,strVhost,strApp,strId));
    }
#endif //defined(ENABLE_MP4V2)
}

MediaRecorder::~MediaRecorder() {
}

void MediaRecorder::inputFrame(const Frame::Ptr &frame) {
#if defined(ENABLE_HLS)
    if (_hlsMaker) {
        _hlsMaker->inputFrame(frame);
    }
#endif //defined(ENABLE_HLS)

#if defined(ENABLE_MP4V2)
    if (_mp4Maker) {
        _mp4Maker->inputFrame(frame);
    }
#endif //defined(ENABLE_MP4V2)
}

void MediaRecorder::addTrack(const Track::Ptr &track) {
#if defined(ENABLE_HLS)
    if (_hlsMaker) {
        _hlsMaker->addTrack(track);
    }
#endif //defined(ENABLE_HLS)

#if defined(ENABLE_MP4RECORD)
    if (_mp4Maker) {
        _mp4Maker->addTrack(track);
    }
#endif //defined(ENABLE_MP4RECORD)
}

} /* namespace mediakit */
