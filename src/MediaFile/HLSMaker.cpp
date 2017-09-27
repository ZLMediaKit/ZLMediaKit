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

#include "HLSMaker.h"
#include "Util/File.h"
#include "Util/uv_errno.h"

using namespace ZL::Util;

namespace ZL {
namespace MediaFile {

HLSMaker::HLSMaker(const string& strM3u8File, const string& strHttpUrl,
		uint32_t ui32BufSize, uint32_t ui32Duration, uint32_t ui32Num) {
	if (ui32BufSize < 16 * 1024) {
		ui32BufSize = 16 * 1024;
	}
	m_ui32BufSize = ui32BufSize;
	if (ui32Duration < 5) {
		ui32Duration = 5;
	}
	if (ui32Num < 2) {
		ui32Num = 2;
	}

	m_ui64TsCnt = 0;
	m_strM3u8File = strM3u8File;
	m_strHttpUrl = strHttpUrl.substr(0, strHttpUrl.find_last_of('/') + 1);
	m_ui32NumSegments = ui32Num;
	m_ui32SegmentDuration = ui32Duration;

	m_strOutputPrefix = strM3u8File.substr(0, strM3u8File.find_last_of('.'));
	m_strFileName = m_strOutputPrefix.substr(m_strOutputPrefix.find_last_of('/') + 1);
	m_strTmpFileName = m_strOutputPrefix + "-0.ts";
	m_ts.init(m_strTmpFileName, m_ui32BufSize);
}


HLSMaker::~HLSMaker() {
	m_ts.clear();
	string strDir = m_strOutputPrefix.substr(0,m_strOutputPrefix.find_last_of('/'));
	File::delete_file(strDir.data());
}

int HLSMaker::write_index_file(int iFirstSegment, unsigned int uiLastSegment,
		int iEnd) {
	FILE *pIndexFp;
	char *pcWriteBuf;
	const char *pcTmpM3u8File = (m_strM3u8File).c_str();
	pIndexFp = File::createfile_file(pcTmpM3u8File, "w");
	if (pIndexFp == NULL) {
		return -1;
	}
	if (iFirstSegment < 0) {
		iFirstSegment = 0;
	}
	if (!pIndexFp) {
		WarnL << "Could not open temporary m3u8 index file (" << pcTmpM3u8File
				<< "), no index file will be created";
		return -1;
	}

	pcWriteBuf = (char *) malloc(sizeof(char) * 1024);
	if (!pcWriteBuf) {
		WarnL << "Could not allocate write buffer for index file, index file will be invalid";
		fclose(pIndexFp);
		return -1;
	}

	if (m_ui32NumSegments) {
		snprintf(pcWriteBuf, 1024,
				"#EXTM3U\n#EXT-X-TARGETDURATION:%u\n#EXT-X-MEDIA-SEQUENCE:%u\n",
				m_ui32SegmentDuration, iFirstSegment);
	} else {
		snprintf(pcWriteBuf, 1024, "#EXTM3U\n#EXT-X-TARGETDURATION:%u\n",
				m_ui32SegmentDuration);
	}
	if (fwrite(pcWriteBuf, strlen(pcWriteBuf), 1, pIndexFp) != 1) {
		WarnL << "Could not write to m3u8 index file, will not continue writing to index file";
		free(pcWriteBuf);
		fclose(pIndexFp);
		return -1;
	}

	for (unsigned int i = iFirstSegment; i < uiLastSegment; i++) {
		snprintf(pcWriteBuf, 1024, "#EXTINF:%u,\n%s%s-%u.ts\n",
				m_ui32SegmentDuration, m_strHttpUrl.c_str(),
				m_strFileName.c_str(), i);
		//printf(options.output_prefix);
		if (fwrite(pcWriteBuf, strlen(pcWriteBuf), 1, pIndexFp) != 1) {
			WarnL << "Could not write to m3u8 index file, will not continue writing to index file";
			free(pcWriteBuf);
			fclose(pIndexFp);
			return -1;
		}
	}

	if (iEnd) {
		snprintf(pcWriteBuf, 1024, "#EXT-X-ENDLIST\n");
		if (fwrite(pcWriteBuf, strlen(pcWriteBuf), 1, pIndexFp) != 1) {
			WarnL << "Could not write last file and endlist tag to m3u8 index file";
			free(pcWriteBuf);
			fclose(pIndexFp);
			return -1;
		}
	}

	free(pcWriteBuf);
	fclose(pIndexFp);

	return 1;
}

void HLSMaker::inputH264(void *data, uint32_t length, uint32_t timeStamp,
		int type) {
	switch (type) {
	case 7: //SPS
		if (m_Timer.elapsedTime() >= m_ui32SegmentDuration * 1000) {
			m_ts.clear();
			m_strTmpFileName = StrPrinter << m_strOutputPrefix << '-' << (++m_ui64TsCnt) << ".ts" << endl;
			if (!m_ts.init(m_strTmpFileName, m_ui32BufSize)) {
				//创建文件失败
				return;
			}
			m_Timer.resetTime();
			removets();
			if (write_index_file(m_ui64TsCnt - m_ui32NumSegments, m_ui64TsCnt, 0) == -1) {
				WarnL << "write_index_file error :" << get_uv_errmsg();
			}
		}
	case 1: //P
			//insert aud frame before p and SPS frame
		m_ts.inputH264("\x0\x0\x0\x1\x9\xf0", 6, timeStamp);
	case 5:		//IDR
	case 8:		//PPS
		m_ts.inputH264((char *) data, length, timeStamp);
		break;
	default:
		break;
	}
}

void HLSMaker::inputAAC(void *data, uint32_t length, uint32_t timeStamp) {
	m_ts.inputAAC((char *) data, length, timeStamp);
}

void HLSMaker::removets() {
	if (m_ui64TsCnt <= m_ui32NumSegments) {
		return;
	}
	File::delete_file( (StrPrinter << m_strOutputPrefix << "-"
						<< m_ui64TsCnt - m_ui32NumSegments - 1
						<< ".ts" << endl).data());
}

} /* namespace MediaFile */
} /* namespace ZL */

