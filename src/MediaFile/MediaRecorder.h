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

#ifndef SRC_MEDIAFILE_MEDIARECORDER_H_
#define SRC_MEDIAFILE_MEDIARECORDER_H_

#include <memory>
#include "Player/PlayerBase.h"
#include "Common/MediaSink.h"
#include "MP4Recorder.h"
#include "HlsRecorder.h"

using namespace toolkit;

namespace mediakit {

class MediaRecorder : public MediaSink{
public:
	typedef std::shared_ptr<MediaRecorder> Ptr;
	MediaRecorder(const string &strVhost,
                  const string &strApp,
                  const string &strId,
                  bool enableHls = true,
                  bool enableMp4 = false);
	virtual ~MediaRecorder();

	/**
       * 输入frame
       * @param frame
       */
	void inputFrame(const Frame::Ptr &frame) override;

	/**
       * 添加track，内部会调用Track的clone方法
       * 只会克隆sps pps这些信息 ，而不会克隆Delegate相关关系
       * @param track
       */
	void addTrack(const Track::Ptr & track) override;

      /**
       * 重置track
       */
      void resetTracks() override;
private:
#if defined(ENABLE_HLS)
	std::shared_ptr<HlsRecorder> _hlsRecorder;
#endif //defined(ENABLE_HLS)

#if defined(ENABLE_MP4RECORD)
	std::shared_ptr<MP4Recorder> _mp4Recorder;
#endif //defined(ENABLE_MP4RECORD)
};

} /* namespace mediakit */

#endif /* SRC_MEDIAFILE_MEDIARECORDER_H_ */
