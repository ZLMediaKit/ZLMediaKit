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
        string params;
        if(enableVhost){
            m3u8FilePath = strVhost + "/" + strApp + "/" + strId + "/hls.m3u8";
            params = string(VHOST_KEY) + "=" + strVhost;
        }else{
            m3u8FilePath = strApp + "/" + strId + "/hls.m3u8";
        }
        m3u8FilePath = File::absolutePath(m3u8FilePath,hlsPath);
        _hlsRecorder.reset(new HlsRecorder(m3u8FilePath,params,hlsBufSize, hlsDuration, hlsNum));
    }
#endif //defined(ENABLE_HLS)

#if defined(ENABLE_MP4RECORD)
    GET_CONFIG(string,recordPath,Record::kFilePath);
    GET_CONFIG(string,recordAppName,Record::kAppName);

    if(enableMp4){
        string mp4FilePath;
        if(enableVhost){
            mp4FilePath = strVhost + "/" + recordAppName + "/" + strApp + "/"  + strId + "/";
        } else {
            mp4FilePath = recordAppName + "/" + strApp + "/"  + strId + "/";
        }
        mp4FilePath = File::absolutePath(mp4FilePath,recordPath);
        _mp4Recorder.reset(new MP4Recorder(mp4FilePath,strVhost,strApp,strId));
    }
#endif //defined(ENABLE_MP4RECORD)
}

MediaRecorder::~MediaRecorder() {
}

void MediaRecorder::inputFrame(const Frame::Ptr &frame) {
#if defined(ENABLE_HLS)
    if (_hlsRecorder) {
        _hlsRecorder->inputFrame(frame);
    }
#endif //defined(ENABLE_HLS)

#if defined(ENABLE_MP4RECORD)
    if (_mp4Recorder) {
        _mp4Recorder->inputFrame(frame);
    }
#endif //defined(ENABLE_MP4RECORD)
}

void MediaRecorder::addTrack(const Track::Ptr &track) {
#if defined(ENABLE_HLS)
    if (_hlsRecorder) {
        _hlsRecorder->addTrack(track);
    }
#endif //defined(ENABLE_HLS)

#if defined(ENABLE_MP4RECORD)
    if (_mp4Recorder) {
        _mp4Recorder->addTrack(track);
    }
#endif //defined(ENABLE_MP4RECORD)
}

void MediaRecorder::resetTracks() {
#if defined(ENABLE_HLS)
    if (_hlsRecorder) {
        _hlsRecorder->resetTracks();
    }
#endif //defined(ENABLE_HLS)

#if defined(ENABLE_MP4RECORD)
    if (_mp4Recorder) {
        _mp4Recorder->resetTracks();
    }
#endif //defined(ENABLE_MP4RECORD)
}

} /* namespace mediakit */
