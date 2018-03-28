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

HLSMaker::HLSMaker(const string& strM3u8File,
                   uint32_t ui32BufSize,
                   uint32_t ui32Duration,
                   uint32_t ui32Num) {
	if (ui32BufSize < 16 * 1024) {
		ui32BufSize = 16 * 1024;
	}
    if(ui32Num < 1){
        ui32Num = 1;
    }
	m_ui32BufSize = ui32BufSize;
	m_ui64TsCnt = 0;
	m_strM3u8File = strM3u8File;
	m_ui32NumSegments = ui32Num;
	m_ui32SegmentDuration = ui32Duration;

	m_strOutputPrefix = strM3u8File.substr(0, strM3u8File.find_last_of('.'));
	m_strFileName = m_strOutputPrefix.substr(m_strOutputPrefix.find_last_of('/') + 1);
	m_ts.init(m_strOutputPrefix + "-0.ts", m_ui32BufSize);
}


HLSMaker::~HLSMaker() {
	m_ts.clear();
	string strDir = m_strOutputPrefix.substr(0,m_strOutputPrefix.find_last_of('/'));
	File::delete_file(strDir.data());
}

bool HLSMaker::write_index_file(int iFirstSegment, unsigned int uiLastSegment, int iEnd) {
	char acWriteBuf[1024];
    std::shared_ptr<FILE> pM3u8File(File::createfile_file(m_strM3u8File.data(), "w"),[](FILE *fp){
        if(fp){
            fflush(fp);
            fclose(fp);
        }
    });
	if (!pM3u8File) {
		WarnL << "Could not open temporary m3u8 index file (" << m_strM3u8File << "), no index file will be created";
		return false;
	}
    if (iFirstSegment < 0) {
        iFirstSegment = 0;
    }

    //最少1秒
    int maxSegmentDuration = 0;
    for (auto dur : m_iDurations) {
        dur /=1000;
        if(dur > maxSegmentDuration){
            maxSegmentDuration = dur;
        }
    }
	if (m_ui32NumSegments) {
        snprintf(acWriteBuf,
                 sizeof(acWriteBuf),
                 "#EXTM3U\n"
                 "#EXT-X-VERSION:3\n"
                 "#EXT-X-ALLOW-CACHE:NO\n"
                 "#EXT-X-TARGETDURATION:%u\n"
                 "#EXT-X-MEDIA-SEQUENCE:%u\n",
                 maxSegmentDuration + 1,
                 iFirstSegment);
	} else {
		snprintf(acWriteBuf,
                 sizeof(acWriteBuf),
                 "#EXTM3U\n"
                 "#EXT-X-VERSION:3\n"
                 "#EXT-X-ALLOW-CACHE:NO\n"
                 "#EXT-X-TARGETDURATION:%u\n",
                 maxSegmentDuration);
	}
	if (fwrite(acWriteBuf, strlen(acWriteBuf), 1, pM3u8File.get()) != 1) {
		WarnL << "Could not write to m3u8 index file, will not continue writing to index file";
        return false;
	}

	for (unsigned int i = iFirstSegment; i < uiLastSegment; i++) {
		snprintf(acWriteBuf,
                 sizeof(acWriteBuf),
                 "#EXTINF:%.3f,\n%s-%u.ts\n",
                 m_iDurations[i-iFirstSegment]/1000.0,
				 m_strFileName.c_str(),
                 i);
		if (fwrite(acWriteBuf, strlen(acWriteBuf), 1, pM3u8File.get()) != 1) {
			WarnL << "Could not write to m3u8 index file, will not continue writing to index file";
            return false;
		}
	}

	if (iEnd) {
		snprintf(acWriteBuf, sizeof(acWriteBuf), "#EXT-X-ENDLIST\n");
		if (fwrite(acWriteBuf, strlen(acWriteBuf), 1, pM3u8File.get()) != 1) {
			WarnL << "Could not write last file and endlist tag to m3u8 index file";
            return false;
		}
	}
	return true;
}

void HLSMaker::inputH264(void *data, uint32_t length, uint32_t timeStamp, int type) {
    if(m_ui32LastStamp == 0){
        m_ui32LastStamp = timeStamp;
    }
    int stampInc = timeStamp - m_ui32LastStamp;

	switch (type) {
	case 7: //SPS
		if (stampInc >= m_ui32SegmentDuration * 1000) {
            m_ui32LastStamp = timeStamp;
            //关闭文件
			m_ts.clear();
			auto strTmpFileName = StrPrinter << m_strOutputPrefix << '-' << (++m_ui64TsCnt) << ".ts" << endl;
			if (!m_ts.init(strTmpFileName, m_ui32BufSize)) {
				//创建文件失败
				return;
			}
            //记录切片时间
            m_iDurations.push_back(stampInc);
			if(removets()){
                //删除老的时间戳
                m_iDurations.pop_front();
            }
			write_index_file(m_ui64TsCnt - m_ui32NumSegments, m_ui64TsCnt, 0);
		}
	case 1: //P
			//insert aud frame before p and SPS frame
		m_ts.inputH264("\x0\x0\x0\x1\x9\xf0", 6, timeStamp * 90);
	case 5:		//IDR
	case 8:		//PPS
		m_ts.inputH264((char *) data, length, timeStamp * 90);
		break;
	default:
		break;
	}
}

void HLSMaker::inputAAC(void *data, uint32_t length, uint32_t timeStamp) {
    m_ts.inputAAC((char *) data, length, timeStamp * 90);
}

bool HLSMaker::removets() {
	if (m_ui64TsCnt < m_ui32NumSegments + 2) {
		return false;
	}
	File::delete_file((StrPrinter << m_strOutputPrefix << "-"
                                  << m_ui64TsCnt - m_ui32NumSegments - 2
                                  << ".ts" << endl).data());
    return true;
}

} /* namespace MediaFile */
} /* namespace ZL */

