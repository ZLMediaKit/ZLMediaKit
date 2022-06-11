/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Recorder.h"
#include "Common/config.h"
#include "Common/MediaSource.h"
#include "MP4Recorder.h"
#include "HlsRecorder.h"

using namespace std;
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
                return File::absolutePath(m3u8FilePath, customized_path);
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
                return File::absolutePath(mp4FilePath, customized_path);
            }
            return File::absolutePath(mp4FilePath, recordPath);
        }
        default:
            return "";
    }
}

std::shared_ptr<MediaSinkInterface> Recorder::createRecorder(type type, const string &vhost, const string &app, const string &stream_id, const string &customized_path, size_t max_second){
    auto path = Recorder::getRecordPath(type, vhost, app, stream_id, customized_path);
    switch (type) {
        case Recorder::type_hls: {
#if defined(ENABLE_HLS)
            GET_CONFIG(bool, enable_vhost, General::kEnableVhost);
            auto ret = std::make_shared<HlsRecorder>(path, enable_vhost ? string(VHOST_KEY) + "=" + vhost : "");
            ret->setMediaSource(vhost, app, stream_id);
            return ret;
#else
            throw std::invalid_argument("hls相关功能未打开，请开启ENABLE_HLS宏后编译再测试");
#endif

        }

        case Recorder::type_mp4: {
#if defined(ENABLE_MP4)
            return std::make_shared<MP4Recorder>(path, vhost, app, stream_id, max_second);
#else
            throw std::invalid_argument("mp4相关功能未打开，请开启ENABLE_MP4宏后编译再测试");
#endif
        }

        default: throw std::invalid_argument("未知的录制类型");
    }
}

} /* namespace mediakit */
