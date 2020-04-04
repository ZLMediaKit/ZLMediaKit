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
#include "Common/MediaSource.h"
#include "MP4Recorder.h"
#include "HlsRecorder.h"

using namespace toolkit;

namespace mediakit {

string Recorder::getRecordPath(Recorder::type type, const string &vhost, const string &app, const string &stream_id, const string &customized_path) {
    GET_CONFIG(bool, enableVhost, General::kEnableVhost);
    switch (type) {
        case Recorder::type_hls: {
            GET_CONFIG(string, hlsPath, Hls::kFilePath);
            string m3u8FilePath;
            if (enableVhost) {
                m3u8FilePath = vhost + "/" + app + "/" + stream_id + "/hls.m3u8";
            } else {
                m3u8FilePath = app + "/" + stream_id + "/hls.m3u8";
            }
            //Here we use the customized file path.
            if (!customized_path.empty()) {
                m3u8FilePath = customized_path + "/hls.m3u8";
            }
            return File::absolutePath(m3u8FilePath, hlsPath);
        }
        case Recorder::type_mp4: {
            GET_CONFIG(string, recordPath, Record::kFilePath);
            GET_CONFIG(string, recordAppName, Record::kAppName);
            string mp4FilePath;
            if (enableVhost) {
                mp4FilePath = vhost + "/" + recordAppName + "/" + app + "/" + stream_id + "/";
            } else {
                mp4FilePath = recordAppName + "/" + app + "/" + stream_id + "/";
            }
            //Here we use the customized file path.
            if (!customized_path.empty()) {
                mp4FilePath = customized_path + "/";
            }
            return File::absolutePath(mp4FilePath, recordPath);
        }
        default:
            return "";
    }
}

std::shared_ptr<MediaSinkInterface> Recorder::createRecorder(type type, const string &vhost, const string &app, const string &stream_id, const string &customized_path){
    auto path = Recorder::getRecordPath(type, vhost, app, stream_id);
    switch (type) {
        case Recorder::type_hls: {
#if defined(ENABLE_HLS)
            auto ret = std::make_shared<HlsRecorder>(path, string(VHOST_KEY) + "=" + vhost);
            ret->setMediaSource(vhost, app, stream_id);
            return ret;
#endif
            return nullptr;
        }

        case Recorder::type_mp4: {
#if defined(ENABLE_MP4)
            return std::make_shared<MP4Recorder>(path, vhost, app, stream_id);
#endif
            return nullptr;
        }

        default:
            return nullptr;
    }
}

} /* namespace mediakit */
