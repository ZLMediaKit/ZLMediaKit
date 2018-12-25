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
#include "Extension/H264.h"
using namespace toolkit;

namespace mediakit {

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
	_ui32BufSize = ui32BufSize;
	_ui64TsCnt = 0;
	_strM3u8File = strM3u8File;
	_ui32NumSegments = ui32Num;
	_ui32SegmentDuration = ui32Duration;
	_ui32LastStamp = 0;

	_strOutputPrefix = strM3u8File.substr(0, strM3u8File.rfind('.'));
	_strFileName = _strOutputPrefix.substr(_strOutputPrefix.rfind('/') + 1);
	_ts.init(_strOutputPrefix + "-0.ts", _ui32BufSize);
}


HLSMaker::~HLSMaker() {
	_ts.clear();
	string strDir = _strOutputPrefix.substr(0,_strOutputPrefix.rfind('/'));
	File::delete_file(strDir.data());
}

bool HLSMaker::write_index_file(int iFirstSegment, unsigned int uiLastSegment, int iEnd) {
	char acWriteBuf[1024];
    std::shared_ptr<FILE> pM3u8File(File::createfile_file(_strM3u8File.data(), "w"),[](FILE *fp){
        if(fp){
            fflush(fp);
            fclose(fp);
        }
    });
	if (!pM3u8File) {
		WarnL << "Could not open temporary m3u8 index file (" << _strM3u8File << "), no index file will be created";
		return false;
	}
    if (iFirstSegment < 0) {
        iFirstSegment = 0;
    }

    //最少1秒
    int maxSegmentDuration = 0;
    for (auto dur : _iDurations) {
        dur /=1000;
        if(dur > maxSegmentDuration){
            maxSegmentDuration = dur;
        }
    }
	if (_ui32NumSegments) {
        snprintf(acWriteBuf,
                 sizeof(acWriteBuf),
                 "#EXTM3U\r\n"
                 "#EXT-X-VERSION:3\r\n"
                 "#EXT-X-ALLOW-CACHE:NO\r\n"
                 "#EXT-X-TARGETDURATION:%u\r\n"
                 "#EXT-X-MEDIA-SEQUENCE:%u\r\n",
                 maxSegmentDuration + 1,
                 iFirstSegment);
	} else {
		snprintf(acWriteBuf,
                 sizeof(acWriteBuf),
                 "#EXTM3U\r\n"
                 "#EXT-X-VERSION:3\r\n"
                 "#EXT-X-ALLOW-CACHE:NO\r\n"
                 "#EXT-X-TARGETDURATION:%u\r\n",
                 maxSegmentDuration);
	}
	if (fwrite(acWriteBuf, strlen(acWriteBuf), 1, pM3u8File.get()) != 1) {
		WarnL << "Could not write to m3u8 index file, will not continue writing to index file";
        return false;
	}

	for (unsigned int i = iFirstSegment; i < uiLastSegment; i++) {
		snprintf(acWriteBuf,
                 sizeof(acWriteBuf),
                 "#EXTINF:%.3f,\r\n%s-%u.ts\r\n",
                 _iDurations[i-iFirstSegment]/1000.0,
				 _strFileName.c_str(),
                 i);
		if (fwrite(acWriteBuf, strlen(acWriteBuf), 1, pM3u8File.get()) != 1) {
			WarnL << "Could not write to m3u8 index file, will not continue writing to index file";
            return false;
		}
	}

	if (iEnd) {
		snprintf(acWriteBuf, sizeof(acWriteBuf), "#EXT-X-ENDLIST\r\n");
		if (fwrite(acWriteBuf, strlen(acWriteBuf), 1, pM3u8File.get()) != 1) {
			WarnL << "Could not write last file and endlist tag to m3u8 index file";
            return false;
		}
	}
	return true;
}

void HLSMaker::inputH264(const Frame::Ptr &frame) {
    auto dts = frame->dts();
    if(_ui32LastStamp == 0){
        _ui32LastStamp = dts;
    }
    int stampInc = dts - _ui32LastStamp;
    auto type =  H264_TYPE(((uint8_t*)(frame->data() + frame->prefixSize()))[0]);
    
	switch (type) {
	case H264Frame::NAL_SPS: //SPS
		if (stampInc >= _ui32SegmentDuration * 1000) {
            _ui32LastStamp = dts;
            //关闭文件
			_ts.clear();
			auto strTmpFileName = StrPrinter << _strOutputPrefix << '-' << (++_ui64TsCnt) << ".ts" << endl;
			if (!_ts.init(strTmpFileName, _ui32BufSize)) {
				//创建文件失败
				return;
			}
            //记录切片时间
            _iDurations.push_back(stampInc);
			if(removets()){
                //删除老的时间戳
                _iDurations.pop_front();
            }
			write_index_file(_ui64TsCnt - _ui32NumSegments, _ui64TsCnt, 0);
		}
	case H264Frame::NAL_B_P: //P
			//insert aud frame before p and SPS frame
		if(dts != _ui32LastFrameStamp){
			_ts.inputH264("\x0\x0\x0\x1\x9\xf0", 6, dts * 90LL , frame->pts() * 90LL);
		}
	case H264Frame::NAL_IDR:		//IDR
	case H264Frame::NAL_PPS:		//PPS
		_ts.inputH264(frame->data(), frame->size(), dts * 90LL , frame->pts() * 90LL);
		break;
	default:
		break;
	}

	_ui32LastFrameStamp = dts;
}

void HLSMaker::inputAAC(const Frame::Ptr &frame) {
    _ts.inputAAC(frame->data(), frame->size(), frame->dts() * 90LL , frame->pts() * 90LL);
}

bool HLSMaker::removets() {
	if (_ui64TsCnt < _ui32NumSegments + 2) {
		return false;
	}
	File::delete_file((StrPrinter << _strOutputPrefix << "-"
                                  << _ui64TsCnt - _ui32NumSegments - 2
                                  << ".ts" << endl).data());
    return true;
}

void HLSMaker::onTrackFrame(const Frame::Ptr &frame) {
	switch (frame->getCodecId()){
		case CodecH264:{
			if( frame->prefixSize() != 0){
				inputH264(frame);
			}else{
				WarnL << "h264必须要有头4个或3个字节的前缀";
			}
		}
			break;
		case CodecAAC:{
			if( frame->prefixSize() == 7) {
				inputAAC(frame);
			}else{
				WarnL << "adts必须要有头7个字节的adts头";
			}
		}
			break;

		default:
			break;
	}
}

} /* namespace mediakit */

