/*
 * HLSMaker.h
 *
 *  Created on: 2013-6-24
 *      Author: root
 */

#ifndef HLSMAKER_H_
#define HLSMAKER_H_

#include "config.h"
#include "TSMaker.h"
#include "Util/TimeTicker.h"
#include "Util/File.h"
#include "Util/util.h"
#include "Util/logger.h"

using namespace ZL::Util;

namespace ZL {
namespace MediaFile {

class HLSMaker {
public:
	HLSMaker(const string &strM3u8File,
			const string &strHttpUrl,
			uint32_t ui32BufSize = 64 * 1024,
			uint32_t ui32Duration = 5,
			uint32_t ui32Num = 3);

	virtual ~HLSMaker();

	//时间戳：参考频率90000
	void inputH264(	void *pData,
					uint32_t ui32Length,
					uint32_t ui32TimeStamp,
					int iType);

	//时间戳：参考频率90000
	void inputAAC(	void *pData,
					uint32_t ui32Length,
					uint32_t ui32TimeStamp);
private:
	TSMaker m_ts;
	string m_strM3u8File;
	string m_strHttpUrl;
	string m_strFileName;
	string m_strOutputPrefix;
	string m_strTmpFileName;
	uint32_t m_ui32SegmentDuration;
	uint32_t m_ui32NumSegments;
	uint64_t m_ui64TsCnt;
	uint32_t m_ui32BufSize;
	Ticker m_Timer;

	int write_index_file(int iFirstSegment, unsigned int uiLastSegment, int iEnd);
	void removets();
};

} /* namespace MediaFile */
} /* namespace ZL */

#endif /* HLSMAKER_H_ */
