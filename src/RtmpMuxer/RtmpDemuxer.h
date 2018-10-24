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

#ifndef SRC_RTMP_RTMPPARSER_H_
#define SRC_RTMP_RTMPPARSER_H_

#include <functional>
#include <unordered_map>
#include "Rtmp/amf.h"
#include "Rtmp/Rtmp.h"
#include "Player/Player.h"
#include "Util/TimeTicker.h"
#include "Player/PlayerBase.h"
using namespace toolkit;

#define H264_CODEC_ID 7
#define AAC_CODEC_ID 10

namespace mediakit {

class RtmpDemuxer : public PlayerBase{
public:
	typedef std::shared_ptr<RtmpDemuxer> Ptr;
	RtmpDemuxer(const AMFValue &val);
	virtual ~RtmpDemuxer();

	bool inputRtmp(const RtmpPacket::Ptr &pkt);

	bool isInited() const override{
        if((_iAudioCodecID | _iVideoCodecID) == 0){
            //音视频codec_id都未获取到，说明还未初始化成功
            return false;
        }
        if((_iAudioCodecID & _iVideoCodecID) == 0 && _ticker.elapsedTime() < 300){
            //音视频codec_id有其一未获取到,且最少分析300ms才能断定没有音频或视频
            return false;
        }
		if (_iAudioCodecID && !_strAudioCfg.size()) {
            //如果音频是aac但是还未获取aac config ，则未初始化成功
			return false;
		}
		if (_iVideoCodecID && !_strSPS.size()) {
            //如果视频是h264但是还未获取sps ，则未初始化成功
            return false;
		}
        //初始化成功
		return true;
	}
	float getDuration() const override{
		return _fDuration;
	}
private:
	inline void onCheckMedia(const AMFValue &obj);

	//返回值：true 代表是i帧第一个rtp包
	inline bool inputVideo(const RtmpPacket::Ptr &pkt);
	inline bool inputAudio(const RtmpPacket::Ptr &pkt);
	inline void _onGetH264(const char *pcData, int iLen, uint32_t ui32TimeStamp);
	inline void onGetH264(const char *pcData, int iLen, uint32_t ui32TimeStamp);
	inline void onGetAAC(const char *pcData, int iLen, uint32_t ui32TimeStamp);
	//video
	H264Frame _h264frame;
	//aduio
	AACFrame _adts;

	int _iSampleRate = 44100;
	int _iSampleBit = 16;
	int _iChannel = 1;

	string _strSPS;
	string _strPPS;
	string _strAudioCfg;
	int _iVideoWidth = 0;
	int _iVideoHeight = 0;
	float _fVideoFps = 0;
    //音视频codec_id初始为0代表尚未获取到
    int _iAudioCodecID = 0;
    int _iVideoCodecID = 0;
	float _fDuration = 0;
    mutable Ticker _ticker;
    function<void(const H264Frame &frame)> onVideo;
	function<void(const AACFrame &frame)> onAudio;
	recursive_mutex _mtxCB;


};

} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPPARSER_H_ */
