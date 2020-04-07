/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_MEDIAFILE_RECORDER_H_
#define SRC_MEDIAFILE_RECORDER_H_
#include <memory>
#include <string>
using namespace std;
namespace mediakit {
class MediaSinkInterface;

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
     * @param customized_path 录像文件保存自定义目录，默认为空则自动生成
     * @return  录制文件绝对路径
     */
    static string getRecordPath(type type, const string &vhost, const string &app, const string &stream_id,const string &customized_path = "");

    /**
     * 创建录制器对象
     * @param type hls还是MP4录制
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param stream_id 流id
     * @param customized_path 录像文件保存自定义目录，默认为空则自动生成
     * @return 对象指针，可能为nullptr
     */
    static std::shared_ptr<MediaSinkInterface> createRecorder(type type, const string &vhost, const string &app, const string &stream_id, const string &customized_path = "");

    /**
     * 获取录制状态
     * @param type hls还是MP4录制
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param stream_id 流id
     * @return 是否真正录制
     */
    static bool isRecording(type type, const string &vhost, const string &app, const string &stream_id);

    /**
     * 开始录制
     * @param type hls还是MP4录制
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param stream_id 流id
     * @param customized_path 录像文件保存自定义目录，默认为空则自动生成
     * @return 成功与否
     */
    static bool startRecord(type type, const string &vhost, const string &app, const string &stream_id,const string &customized_path);

    /**
     * 停止录制
     * @param type hls还是MP4录制
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param stream_id 流id
     */
    static bool stopRecord(type type, const string &vhost, const string &app, const string &stream_id);

private:
    Recorder() = delete;
    ~Recorder() = delete;
};

} /* namespace mediakit */
#endif /* SRC_MEDIAFILE_RECORDER_H_ */
