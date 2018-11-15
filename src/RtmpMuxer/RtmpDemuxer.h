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

#ifndef SRC_RTMP_RTMPDEMUXER_H_
#define SRC_RTMP_RTMPDEMUXER_H_

#include <functional>
#include <unordered_map>
#include "Rtmp/amf.h"
#include "Rtmp/Rtmp.h"
#include "Player/PlayerBase.h"
#include "Util/TimeTicker.h"
#include "RtmpCodec.h"

using namespace toolkit;

namespace mediakit {

class RtmpDemuxer : public Demuxer{
public:
	typedef std::shared_ptr<RtmpDemuxer> Ptr;

	/**
	 * 等效于RtmpDemuxer(AMFValue(AMF_NULL))
	 */
	RtmpDemuxer(){}
	/**
	 * 构造rtmp解复用器
	 * @param val rtmp的metedata，可以传入null类型，
	 * 这样就会在inputRtmp时异步探测媒体编码格式
	 */
	RtmpDemuxer(const AMFValue &val);
	virtual ~RtmpDemuxer(){};

	/**
	 *
	 * 获取rtmp track 数
	 * @param metedata rtmp的metedata
	 * @return
	 */
	static int getTrackCount(const AMFValue &metedata);

	/**
	 * 开始解复用
	 * @param pkt rtmp包
	 * @return true 代表是i帧
	 */
	bool inputRtmp(const RtmpPacket::Ptr &pkt);
private:
	void makeVideoTrack(const AMFValue &val);
	void makeAudioTrack(const AMFValue &val);
private:
	bool _tryedGetVideoTrack = false;
	bool _tryedGetAudioTrack = false;
	RtmpCodec::Ptr _audioRtmpDecoder;
	RtmpCodec::Ptr _videoRtmpDecoder;
};

} /* namespace mediakit */

















#endif /* SRC_RTMP_RTMPDEMUXER_H_ */
