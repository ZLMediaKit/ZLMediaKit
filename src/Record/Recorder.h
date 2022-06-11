/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_MEDIAFILE_RECORDER_H_
#define SRC_MEDIAFILE_RECORDER_H_
#include <memory>
#include <string>

namespace mediakit {
class MediaSinkInterface;

class RecordInfo {
public:
    time_t start_time;  // GMT 标准时间，单位秒
    float time_len;     // 录像长度，单位秒
    off_t file_size;    // 文件大小，单位 BYTE
    std::string file_path;   // 文件路径
    std::string file_name;   // 文件名称
    std::string folder;      // 文件夹路径
    std::string url;         // 播放路径
    std::string app;         // 应用名称
    std::string stream;      // 流 ID
    std::string vhost;       // 虚拟主机
};

class Recorder{
public:
    typedef enum {
        // 录制hls
        type_hls = 0,
        // 录制MP4
        type_mp4 = 1
    } type;

    /**
     * 获取录制文件绝对路径
     * @param type hls还是MP4录制
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param stream_id 流id
     * @param customized_path 录像文件保存自定义根目录，为空则采用配置文件设置
     * @return  录制文件绝对路径
     */
    static std::string getRecordPath(type type, const std::string &vhost, const std::string &app, const std::string &stream_id,const std::string &customized_path = "");

    /**
     * 创建录制器对象
     * @param type hls还是MP4录制
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param stream_id 流id
     * @param customized_path 录像文件保存自定义根目录，为空则采用配置文件设置
     * @param max_second mp4录制最大切片时间，单位秒，置0则采用配置文件配置
     * @return 对象指针，可能为nullptr
     */
    static std::shared_ptr<MediaSinkInterface> createRecorder(type type, const std::string &vhost, const std::string &app, const std::string &stream_id, const std::string &customized_path = "", size_t max_second = 0);

private:
    Recorder() = delete;
    ~Recorder() = delete;
};

} /* namespace mediakit */
#endif /* SRC_MEDIAFILE_RECORDER_H_ */
