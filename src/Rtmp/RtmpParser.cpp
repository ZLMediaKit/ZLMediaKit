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

#include "RtmpParser.h"

namespace mediakit {

RtmpParser::RtmpParser(const AMFValue &val) {
	auto videoCodec = val["videocodecid"];
	auto audioCodec = val["audiocodecid"];
    
	if (videoCodec.type() == AMF_STRING) {
        if (videoCodec.as_string() == "avc1") {
            //h264
            _iVideoCodecID = H264_CODEC_ID;
        } else {
            InfoL << "不支持RTMP视频格式:" << videoCodec.as_string();
        }
    }else if (videoCodec.type() != AMF_NULL){
        _iVideoCodecID = videoCodec.as_integer();
        if (_iVideoCodecID != H264_CODEC_ID) {
            InfoL << "不支持RTMP视频格式:" << videoCodec.as_integer();
        }
    }

	if (audioCodec.type() == AMF_STRING) {
		if (audioCodec.as_string() == "mp4a") {
			//aac
            _iAudioCodecID = AAC_CODEC_ID;
		} else {
			InfoL << "不支持RTMP音频格式:" << audioCodec.as_string();
		}
    }else if (audioCodec.type() != AMF_NULL) {
        _iAudioCodecID = audioCodec.as_integer();
        if (_iAudioCodecID != AAC_CODEC_ID) {
            InfoL << "不支持RTMP音频格式:" << audioCodec.as_integer();
        }
    }
	onCheckMedia(val);
}

RtmpParser::~RtmpParser() {
}

bool RtmpParser::inputRtmp(const RtmpPacket::Ptr &pkt) {
    switch (pkt->typeId) {
        case MSG_VIDEO:{
            if(_iVideoCodecID == 0){
                //未初始化视频
                _iVideoCodecID = pkt->getMediaType();
                if(_iVideoCodecID != H264_CODEC_ID){
                    InfoL << "不支持RTMP视频格式:" << _iVideoCodecID;
                }
            }
            if(_iVideoCodecID == H264_CODEC_ID){
                return inputVideo(pkt);
            }
            return false;
        }

        case MSG_AUDIO: {
            if(_iAudioCodecID == 0){
                //未初始化音频
                _iAudioCodecID = pkt->getMediaType();
                if(_iAudioCodecID != AAC_CODEC_ID){
                    InfoL << "不支持RTMP音频格式:" << _iAudioCodecID;
                }
            }
            if (_iAudioCodecID == AAC_CODEC_ID) {
                return inputAudio(pkt);
            }
            return false;
        }

        default:
            return false;
    }
}

inline bool RtmpParser::inputVideo(const RtmpPacket::Ptr &pkt) {
	if (pkt->isCfgFrame()) {
		//WarnL << " got h264 cfg";
		if (_strSPS.size()) {
			return false;
		}
		_strSPS.assign("\x00\x00\x00\x01", 4);
		_strSPS.append(pkt->getH264SPS());

		_strPPS.assign("\x00\x00\x00\x01", 4);
		_strPPS.append(pkt->getH264PPS());

		getAVCInfo(pkt->getH264SPS(), _iVideoWidth, _iVideoHeight, _fVideoFps);
		return false;
	}

	if (_strSPS.size()) {
		uint32_t iTotalLen = pkt->strBuf.size();
		uint32_t iOffset = 5;
		while(iOffset + 4 < iTotalLen){
            uint32_t iFrameLen;
            memcpy(&iFrameLen, pkt->strBuf.data() + iOffset, 4);
            iFrameLen = ntohl(iFrameLen);
			iOffset += 4;
			if(iFrameLen + iOffset > iTotalLen){
				break;
			}
			_onGetH264(pkt->strBuf.data() + iOffset, iFrameLen, pkt->timeStamp);
			iOffset += iFrameLen;
		}
	}
	return  pkt->isVideoKeyFrame();
}
inline void RtmpParser::_onGetH264(const char* pcData, int iLen, uint32_t ui32TimeStamp) {
	switch (pcData[0] & 0x1F) {
	case 5: {
		onGetH264(_strSPS.data() + 4, _strSPS.length() - 4, ui32TimeStamp);
		onGetH264(_strPPS.data() + 4, _strPPS.length() - 4, ui32TimeStamp);
	}
	case 1: {
		onGetH264(pcData, iLen, ui32TimeStamp);
	}
		break;
	default:
		//WarnL <<(int)(pcData[0] & 0x1F);
		break;
	}
}
inline void RtmpParser::onGetH264(const char* pcData, int iLen, uint32_t ui32TimeStamp) {
	_h264frame.type = pcData[0] & 0x1F;
	_h264frame.timeStamp = ui32TimeStamp;
	_h264frame.buffer.assign("\x0\x0\x0\x1", 4);  //添加264头
	_h264frame.buffer.append(pcData, iLen);
	{
		lock_guard<recursive_mutex> lck(_mtxCB);
		if (onVideo) {
			onVideo(_h264frame);
		}
	}
	_h264frame.buffer.clear();
}

inline bool RtmpParser::inputAudio(const RtmpPacket::Ptr &pkt) {
	if (pkt->isCfgFrame()) {
		if (_strAudioCfg.size()) {
			return false;
		}
		_strAudioCfg = pkt->getAacCfg();
		_iSampleBit = pkt->getAudioSampleBit();
		makeAdtsHeader(_strAudioCfg,_adts);
		getAACInfo(_adts, _iSampleRate, _iChannel);
		return false;
	}
	if (_strAudioCfg.size()) {
		onGetAAC(pkt->strBuf.data() + 2, pkt->strBuf.size() - 2, pkt->timeStamp);
	}
	return false;
}
inline void RtmpParser::onGetAAC(const char* pcData, int iLen, uint32_t ui32TimeStamp) {
    if(iLen + 7 > sizeof(_adts.buffer)){
        WarnL << "Illegal adts data, exceeding the length limit.";
        return;
    }
	//添加adts头
	memcpy(_adts.buffer + 7, pcData, iLen);
	_adts.aac_frame_length = 7 + iLen;
    _adts.timeStamp = ui32TimeStamp;
    writeAdtsHeader(_adts, _adts.buffer);
	{
		lock_guard<recursive_mutex> lck(_mtxCB);
		if (onAudio) {
			onAudio(_adts);
		}
	}
	_adts.aac_frame_length = 7;

}
inline void RtmpParser::onCheckMedia(const AMFValue& obj) {
	obj.object_for_each([&](const string &key ,const AMFValue& val) {
		if(key == "duration") {
			_fDuration = val.as_number();
			return;
		}
		if(key == "width") {
			_iVideoWidth = val.as_number();
			return;
		}
		if(key == "height") {
			_iVideoHeight = val.as_number();
			return;
		}
		if(key == "framerate") {
			_fVideoFps = val.as_number();
			return;
		}
		if(key == "audiosamplerate") {
			_iSampleRate = val.as_number();
			return;
		}
		if(key == "audiosamplesize") {
			_iSampleBit = val.as_number();
			return;
		}
		if(key == "stereo") {
			_iChannel = val.as_boolean() ? 2 :1;
			return;
		}
	});
}


} /* namespace mediakit */
