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
        // 未录制
        status_not_record = 0,
        // 等待MediaSource注册，注册成功后立即开始录制
        status_wait_record = 1,
        // MediaSource已注册，并且正在录制
        status_recording = 2,
    } status;

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
     * 获取录制状态
     * @param type hls还是MP4录制
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param stream_id 流id
     * @return 录制状态
     */
    static status getRecordStatus(type type, const string &vhost, const string &app, const string &stream_id);

    /**
     * 开始录制
     * @param type hls还是MP4录制
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param stream_id 流id
     * @param customized_path 录像文件保存自定义目录，默认为空则自动生成
     * @param waitForRecord 是否等待流注册后再录制，未注册时，置false将返回失败
     * @param continueRecord 流注销时是否继续等待录制还是立即停止录制
     * @return 0代表成功，负数代表失败
     */
    static int startRecord(type type, const string &vhost, const string &app, const string &stream_id,const string &customized_path,bool waitForRecord, bool continueRecord);

    /**
     * 停止录制
     * @param type hls还是MP4录制
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param stream_id 流id
     */
    static bool stopRecord(type type, const string &vhost, const string &app, const string &stream_id);

    /**
     * 停止所有录制，一般程序退出时调用
     */
    static void stopAll();

    /**
     * 获取录制对象
     * @param type hls还是MP4录制
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param stream_id 流id
     */
    static std::shared_ptr<MediaSinkInterface> getRecorder(type type, const string &vhost, const string &app, const string &stream_id);

    /**
     * 创建录制器对象
     * @param type hls还是MP4录制
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param stream_id 流id
     * @param customized_path 录像文件保存自定义目录，默认为空则自动生成
     * @return 对象指针，可能为nullptr
     */
    static std::shared_ptr<MediaSinkInterface> createRecorder(type type, const string &vhost, const string &app, const string &stream_id, const string &customized_path);
private:
    Recorder() = delete;
    ~Recorder() = delete;
};

} /* namespace mediakit */

#endif /* SRC_MEDIAFILE_RECORDER_H_ */
