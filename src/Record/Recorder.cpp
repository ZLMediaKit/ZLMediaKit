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
#include "Util/File.h"
#include "Common/MediaSource.h"
#include "MP4Recorder.h"
#include "HlsRecorder.h"
#include "FMP4/FMP4MediaSourceMuxer.h"
#include "TS/TSMediaSourceMuxer.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

string Recorder::getRecordPath(Recorder::type type, const MediaTuple& tuple, const string &customized_path) {
    GET_CONFIG(bool, enableVhost, General::kEnableVhost);
    switch (type) {
        case Recorder::type_hls: {
            GET_CONFIG(string, hlsPath, Protocol::kHlsSavePath);
            string m3u8FilePath;
            if (enableVhost) {
                m3u8FilePath = tuple.shortUrl() + "/hls.m3u8";
            } else {
                m3u8FilePath = tuple.app + "/" + tuple.stream + "/hls.m3u8";
            }
            //Here we use the customized file path.
            if (!customized_path.empty()) {
                return File::absolutePath(m3u8FilePath, customized_path);
            }
            return File::absolutePath(m3u8FilePath, hlsPath);
        }
        case Recorder::type_mp4: {
            GET_CONFIG(string, recordPath, Protocol::kMP4SavePath);
            GET_CONFIG(string, recordAppName, Record::kAppName);
            string mp4FilePath;
            if (enableVhost) {
                mp4FilePath = tuple.vhost + "/" + recordAppName + "/" + tuple.app + "/" + tuple.stream + "/";
            } else {
                mp4FilePath = recordAppName + "/" + tuple.app + "/" + tuple.stream + "/";
            }
            //Here we use the customized file path.
            if (!customized_path.empty()) {
                return File::absolutePath(mp4FilePath, customized_path);
            }
            return File::absolutePath(mp4FilePath, recordPath);
        }
        case Recorder::type_hls_fmp4: {
            GET_CONFIG(string, hlsPath, Protocol::kHlsSavePath);
            string m3u8FilePath;
            if (enableVhost) {
                m3u8FilePath = tuple.shortUrl() + "/hls.fmp4.m3u8";
            } else {
                m3u8FilePath = tuple.app + "/" + tuple.stream + "/hls.fmp4.m3u8";
            }
            // Here we use the customized file path.
            if (!customized_path.empty()) {
                return File::absolutePath(m3u8FilePath, customized_path);
            }
            return File::absolutePath(m3u8FilePath, hlsPath);
        }
        default:
            return "";
    }
}

std::shared_ptr<MediaSinkInterface> Recorder::createRecorder(type type, const MediaTuple& tuple, const ProtocolOption &option){
    switch (type) {
        case Recorder::type_hls: {
#if defined(ENABLE_HLS)
            auto path = Recorder::getRecordPath(type, tuple, option.hls_save_path);
            GET_CONFIG(bool, enable_vhost, General::kEnableVhost);
            auto ret = std::make_shared<HlsRecorder>(path, enable_vhost ? string(VHOST_KEY) + "=" + tuple.vhost : "", option);
            ret->setMediaSource(tuple);
            return ret;
#else
            throw std::invalid_argument("hls相关功能未打开，请开启ENABLE_HLS宏后编译再测试");
#endif

        }

        case Recorder::type_mp4: {
#if defined(ENABLE_MP4)
            auto path = Recorder::getRecordPath(type, tuple, option.mp4_save_path);
            return std::make_shared<MP4Recorder>(path, tuple.vhost, tuple.app, tuple.stream, option.mp4_max_second);
#else
            throw std::invalid_argument("mp4相关功能未打开，请开启ENABLE_MP4宏后编译再测试");
#endif
        }

        case Recorder::type_hls_fmp4: {
#if defined(ENABLE_HLS_FMP4)
            auto path = Recorder::getRecordPath(type, tuple, option.hls_save_path);
            GET_CONFIG(bool, enable_vhost, General::kEnableVhost);
            auto ret = std::make_shared<HlsFMP4Recorder>(path, enable_vhost ? string(VHOST_KEY) + "=" + tuple.vhost : "", option);
            ret->setMediaSource(tuple);
            return ret;
#else
            throw std::invalid_argument("hls.fmp4相关功能未打开，请开启ENABLE_HLS_FMP4宏后编译再测试");
#endif
        }

        case Recorder::type_fmp4: {
#if defined(ENABLE_HLS_FMP4) || defined(ENABLE_MP4)
            return std::make_shared<FMP4MediaSourceMuxer>(tuple, option);
#else
            throw std::invalid_argument("fmp4相关功能未打开，请开启ENABLE_HLS_FMP4或ENABLE_MP4宏后编译再测试");
#endif
        }

        case Recorder::type_ts: {
#if defined(ENABLE_HLS) || defined(ENABLE_RTPPROXY)
            return std::make_shared<TSMediaSourceMuxer>(tuple, option);
#else
            throw std::invalid_argument("mpegts相关功能未打开，请开启ENABLE_HLS或ENABLE_RTPPROXY宏后编译再测试");
#endif
        }

        default: throw std::invalid_argument("未知的录制类型");
    }
}

} /* namespace mediakit */
