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

#include "Recorder.h"
#include "Common/config.h"
#include "Http/HttpSession.h"
#include "Util/util.h"
#include "Util/mini.h"
#include "Network/sockutil.h"
#include "HlsMakerImp.h"
#include "Player/PlayerBase.h"
#include "Common/MediaSink.h"
#include "MP4Recorder.h"
#include "HlsRecorder.h"

using namespace toolkit;

namespace mediakit {

MediaSinkInterface *Recorder::createHlsRecorder(const string &strVhost_tmp, const string &strApp, const string &strId) {
#if defined(ENABLE_HLS)
    GET_CONFIG(bool, enableVhost, General::kEnableVhost);
    GET_CONFIG(string, hlsPath, Hls::kFilePath);

    string strVhost = strVhost_tmp;
    if (trim(strVhost).empty()) {
        //如果strVhost为空，则强制为默认虚拟主机
        strVhost = DEFAULT_VHOST;
    }

    string m3u8FilePath;
    string params;
    if (enableVhost) {
        m3u8FilePath = strVhost + "/" + strApp + "/" + strId + "/hls.m3u8";
        params = string(VHOST_KEY) + "=" + strVhost;
    } else {
        m3u8FilePath = strApp + "/" + strId + "/hls.m3u8";
    }
    m3u8FilePath = File::absolutePath(m3u8FilePath, hlsPath);
    return new HlsRecorder(m3u8FilePath, params);
#else
    return nullptr;
#endif //defined(ENABLE_HLS)
}

MediaSinkInterface *Recorder::createMP4Recorder(const string &strVhost_tmp, const string &strApp, const string &strId) {
#if defined(ENABLE_MP4RECORD)
    GET_CONFIG(bool, enableVhost, General::kEnableVhost);
    GET_CONFIG(string, recordPath, Record::kFilePath);
    GET_CONFIG(string, recordAppName, Record::kAppName);

    string strVhost = strVhost_tmp;
    if (trim(strVhost).empty()) {
        //如果strVhost为空，则强制为默认虚拟主机
        strVhost = DEFAULT_VHOST;
    }

    string mp4FilePath;
    if (enableVhost) {
        mp4FilePath = strVhost + "/" + recordAppName + "/" + strApp + "/" + strId + "/";
    } else {
        mp4FilePath = recordAppName + "/" + strApp + "/" + strId + "/";
    }
    mp4FilePath = File::absolutePath(mp4FilePath, recordPath);
    return new MP4Recorder(mp4FilePath, strVhost, strApp, strId);
#else
    return nullptr;
#endif //defined(ENABLE_MP4RECORD)
}


} /* namespace mediakit */
