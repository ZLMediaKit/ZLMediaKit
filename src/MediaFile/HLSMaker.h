/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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

#ifndef HLSMAKER_H_
#define HLSMAKER_H_

#include <deque>
#include "TSMaker.h"
#include "Util/TimeTicker.h"
#include "Util/File.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Common/config.h"
#include "Common/MediaSink.h"
#include "Extension/Frame.h"

using namespace toolkit;

namespace mediakit {

class HLSMaker : public MediaSink{
public:
	HLSMaker(const string &strM3u8File,
			uint32_t ui32BufSize = 64 * 1024,
			uint32_t ui32Duration = 5,
			uint32_t ui32Num = 3);

	virtual ~HLSMaker();

protected:
	/**
     * 某Track输出frame，在onAllTrackReady触发后才会调用此方法
     * @param frame
     */
	void onTrackFrame(const Frame::Ptr &frame) override ;
private:
	void inputH264(const Frame::Ptr &frame);
	void inputAAC(const Frame::Ptr &frame);

	bool write_index_file(int iFirstSegment,
						  unsigned int uiLastSegment,
						  int iEnd);

	bool removets();
private:
	TSMaker _ts;
	string _strM3u8File;
	string _strFileName;
	string _strOutputPrefix;
	uint32_t _ui32SegmentDuration;
	uint32_t _ui32NumSegments;
	uint64_t _ui64TsCnt;
	uint32_t _ui32BufSize;
    uint32_t _ui32LastStamp;
	uint32_t _ui32LastFrameStamp = 0;
	std::deque<int> _iDurations;


};

} /* namespace mediakit */

#endif /* HLSMAKER_H_ */
