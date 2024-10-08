﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_MEDIAFILE_RECORDER_H_
#define SRC_MEDIAFILE_RECORDER_H_

#include <memory>
#include <string>

namespace mediakit {
class MediaSinkInterface;
class ProtocolOption;

struct MediaTuple {
    std::string vhost;
    std::string app;
    std::string stream;
    std::string params;
    std::string shortUrl() const {
        return vhost + '/' + app + '/' + stream;
    }
};

class RecordInfo: public MediaTuple {
public:
    time_t start_time;  // GMT 标准时间，单位秒
    float time_len;     // 录像长度，单位秒
    uint64_t file_size;    // 文件大小，单位 BYTE
    std::string file_path;   // 文件路径
    std::string file_name;   // 文件名称
    std::string folder;      // 文件夹路径
    std::string url;         // 播放路径
};

class Recorder{
public:
    typedef enum {
        // 录制hls  [AUTO-TRANSLATED:24a50dff]
        // Record hls
        type_hls = 0,
        // 录制MP4  [AUTO-TRANSLATED:03d73bb7]
        // Record MP4
        type_mp4 = 1,
        // 录制hls.fmp4  [AUTO-TRANSLATED:031cf6f1]
        // Record hls.fmp4
        type_hls_fmp4 = 2,
        // fmp4直播  [AUTO-TRANSLATED:ac37a248]
        // fmp4 live
        type_fmp4 = 3,
        // ts直播  [AUTO-TRANSLATED:b062b43a]
        // ts live
        type_ts = 4,
    } type;

    /**
     * 获取录制文件绝对路径
     * @param type hls还是MP4录制
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param stream_id 流id
     * @param customized_path 录像文件保存自定义根目录，为空则采用配置文件设置
     * @return  录制文件绝对路径
     * Get the absolute path of the recording file
     * @param type hls or MP4 recording
     * @param vhost virtual host
     * @param app application name
     * @param stream_id stream id
     * @param customized_path custom root directory for saving recording files, empty means using configuration file settings
     * @return  absolute path of the recording file
     
     * [AUTO-TRANSLATED:2fd57fcd]
     */
    static std::string getRecordPath(type type, const MediaTuple& tuple, const std::string &customized_path = "");

    /**
     * 创建录制器对象
     * @param type hls还是MP4录制
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param stream_id 流id
     * @param customized_path 录像文件保存自定义根目录，为空则采用配置文件设置
     * @param max_second mp4录制最大切片时间，单位秒，置0则采用配置文件配置
     * @return 对象指针，可能为nullptr
     * Create a recorder object
     * @param type hls or MP4 recording
     * @param vhost virtual host
     * @param app application name
     * @param stream_id stream id
     * @param customized_path custom root directory for saving recording files, empty means using configuration file settings
     * @param max_second maximum slice time for mp4 recording, in seconds, 0 means using configuration file settings
     * @return object pointer, may be nullptr
     
     
     * [AUTO-TRANSLATED:e0b6e43b]
     */
    static std::shared_ptr<MediaSinkInterface> createRecorder(type type, const MediaTuple& tuple, const ProtocolOption &option);

private:
    Recorder() = delete;
    ~Recorder() = delete;
};

} /* namespace mediakit */
#endif /* SRC_MEDIAFILE_RECORDER_H_ */
