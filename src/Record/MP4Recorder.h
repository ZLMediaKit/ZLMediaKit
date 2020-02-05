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

#ifndef MP4MAKER_H_
#define MP4MAKER_H_

#include <mutex>
#include <memory>
#include "Player/PlayerBase.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Util/TimeTicker.h"
#include "Common/MediaSink.h"
#include "MP4Muxer.h"
using namespace toolkit;

namespace mediakit {

class MP4Info {
public:
	time_t ui64StartedTime; //GMT标准时间，单位秒
	time_t ui64TimeLen;//录像长度，单位秒
	off_t ui64FileSize;//文件大小，单位BYTE
	string strFilePath;//文件路径
	string strFileName;//文件名称
	string strFolder;//文件夹路径
	string strUrl;//播放路径
	string strAppName;//应用名称
	string strStreamId;//流ID
	string strVhost;//vhost
};

#ifdef ENABLE_MP4RECORD
class MP4Recorder : public MediaSinkInterface{
public:
	typedef std::shared_ptr<MP4Recorder> Ptr;

	MP4Recorder(const string &strPath,
				const string &strVhost,
				const string &strApp,
				const string &strStreamId);
	virtual ~MP4Recorder();

	/**
	 * 重置所有Track
	 */
	void resetTracks() override;

	/**
     * 输入frame
     */
	void inputFrame(const Frame::Ptr &frame) override;

    /**
     * 添加ready状态的track
     */
    void addTrack(const Track::Ptr & track) override;
private:
    void createFile();
    void closeFile();
    void asyncClose();
private:
	string _strPath;
	string _strFile;
	string _strFileTmp;
	Ticker _createFileTicker;
	MP4Info _info;
	bool _haveVideo = false;
	MP4MuxerFile::Ptr _muxer;
	list<Track::Ptr> _tracks;
};

#endif ///ENABLE_MP4RECORD

} /* namespace mediakit */

#endif /* MP4MAKER_H_ */
