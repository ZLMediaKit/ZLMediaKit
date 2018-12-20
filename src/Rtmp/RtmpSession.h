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

#ifndef SRC_RTMP_RTMPSESSION_H_
#define SRC_RTMP_RTMPSESSION_H_

#include <unordered_map>
#include "amf.h"
#include "Rtmp.h"
#include "utils.h"
#include "Common/config.h"
#include "RtmpProtocol.h"
#include "RtmpToRtspMediaSource.h"
#include "Util/util.h"
#include "Util/TimeTicker.h"
#include "Network/TcpSession.h"
using namespace toolkit;

namespace mediakit {

class RtmpSession: public TcpSession ,public  RtmpProtocol , public MediaSourceEvent{
public:
	typedef std::shared_ptr<RtmpSession> Ptr;
	RtmpSession(const Socket::Ptr &_sock);
	virtual ~RtmpSession();
	void onRecv(const Buffer::Ptr &pBuf) override;
	void onError(const SockException &err) override;
	void onManager() override;
private:
	void onProcessCmd(AMFDecoder &dec);
	void onCmd_connect(AMFDecoder &dec);
	void onCmd_createStream(AMFDecoder &dec);

	void onCmd_publish(AMFDecoder &dec);
	void onCmd_deleteStream(AMFDecoder &dec);

	void onCmd_play(AMFDecoder &dec);
	void onCmd_play2(AMFDecoder &dec);
	void doPlay(AMFDecoder &dec);
	void doPlayResponse(const string &err,const std::function<void(bool)> &cb);
	void sendPlayResponse(const string &err,const RtmpMediaSource::Ptr &src);

	void onCmd_seek(AMFDecoder &dec);
	void onCmd_pause(AMFDecoder &dec);
	void setMetaData(AMFDecoder &dec);

	void onSendMedia(const RtmpPacket::Ptr &pkt);
	void onSendRawData(const Buffer::Ptr &buffer) override{
        _ui64TotalBytes += buffer->size();
		send(buffer);
	}
	void onRtmpChunk(RtmpPacket &chunkData) override;

	template<typename first, typename second>
	inline void sendReply(const char *str, const first &reply, const second &status) {
		AMFEncoder invoke;
		invoke << str << _dNowReqID << reply << status;
		sendResponse(MSG_CMD, invoke.data());
	}

    bool close() override {
        InfoL << "kick out:" << _mediaInfo._vhost << " " << _mediaInfo._app << " " << _mediaInfo._streamid;
        safeShutdown();
        return true;
    }

    void doDelay(int delaySec,const std::function<void()> &fun);
	void cancelDelyaTask();
	void findStream(const function<void(const RtmpMediaSource::Ptr &src)> &cb ,bool retry = true);
private:
	std::string _strTcUrl;
	MediaInfo _mediaInfo;
	double _dNowReqID = 0;
	Ticker _ticker;//数据接收时间
	SmoothTicker _stampTicker[2];//时间戳生产器
	RingBuffer<RtmpPacket::Ptr>::RingReader::Ptr _pRingReader;
	std::shared_ptr<RtmpMediaSource> _pPublisherSrc;
	std::weak_ptr<RtmpMediaSource> _pPlayerSrc;
	uint32_t _aui32FirstStamp[2] = {0};
	//消耗的总流量
	uint64_t _ui64TotalBytes = 0;
    std::function<void()> _delayTask;
    uint32_t _iTaskTimeLine = 0;

};

} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPSESSION_H_ */
