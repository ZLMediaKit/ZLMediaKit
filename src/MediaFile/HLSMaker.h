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

#include "TSMaker.h"
#include "Common/config.h"
#include "Util/TimeTicker.h"
#include "Util/File.h"
#include "Util/util.h"
#include "Util/logger.h"
#include <deque>

using namespace ZL::Util;

namespace ZL {
namespace MediaFile {

class HLSMaker {
public:
	HLSMaker(const string &strM3u8File,
			uint32_t ui32BufSize = 64 * 1024,
			uint32_t ui32Duration = 5,
			uint32_t ui32Num = 3);

	virtual ~HLSMaker();

	//时间戳：参考频率1000
	void inputH264(	void *pData,
					uint32_t ui32Length,
					uint32_t ui32TimeStamp,
					int iType);

	//时间戳：参考频率1000
	void inputAAC(	void *pData,
					uint32_t ui32Length,
					uint32_t ui32TimeStamp);
private:
	TSMaker m_ts;
	string m_strM3u8File;
	string m_strFileName;
	string m_strOutputPrefix;
	uint32_t m_ui32SegmentDuration;
	uint32_t m_ui32NumSegments;
	uint64_t m_ui64TsCnt;
	uint32_t m_ui32BufSize;
    uint32_t m_ui32LastStamp;
	std::deque<int> m_iDurations;

	bool write_index_file(int iFirstSegment, unsigned int uiLastSegment, int iEnd);
	bool removets();
};

} /* namespace MediaFile */
} /* namespace ZL */

#endif /* HLSMAKER_H_ */
