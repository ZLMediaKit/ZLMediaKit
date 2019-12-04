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

	static status getRecordStatus(type type, const string &vhost, const string &app, const string &stream_id);
	static int startRecord(type type, const string &vhost, const string &app, const string &stream_id,bool waitForRecord, bool continueRecord);
	static void stopRecord(type type, const string &vhost, const string &app, const string &stream_id);
	static void stopAll();
private:
	Recorder() = delete;
	~Recorder() = delete;
};

} /* namespace mediakit */

#endif /* SRC_MEDIAFILE_RECORDER_H_ */
