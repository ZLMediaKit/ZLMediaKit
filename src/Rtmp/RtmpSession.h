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

using namespace ZL::Util;
using namespace ZL::Network;

namespace ZL {
namespace Rtmp {

class RtmpSession: public TcpSession ,public  RtmpProtocol , public MediaSourceEvent{
public:
	typedef std::shared_ptr<RtmpSession> Ptr;
	RtmpSession(const std::shared_ptr<ThreadPool> &_th, const Socket::Ptr &_sock);
	virtual ~RtmpSession();
	void onRecv(const Buffer::Ptr &pBuf) override;
	void onError(const SockException &err) override;
	void onManager() override;
private:
    std::string m_strTcUrl;
    MediaInfo m_mediaInfo;
	double m_dNowReqID = 0;
	Ticker m_ticker;//数据接收时间
	SmoothTicker m_stampTicker[2];//时间戳生产器
	typedef void (RtmpSession::*rtmpCMDHandle)(AMFDecoder &dec);
	static unordered_map<string, rtmpCMDHandle> g_mapCmd;

	RingBuffer<RtmpPacket::Ptr>::RingReader::Ptr m_pRingReader;
	std::shared_ptr<RtmpMediaSource> m_pPublisherSrc;
	bool m_bPublisherSrcRegisted = false;
    std::weak_ptr<RtmpMediaSource> m_pPlayerSrc;
    uint32_t m_aui32FirstStamp[2] = {0};
    //消耗的总流量
    uint64_t m_ui64TotalBytes = 0;
	void onProcessCmd(AMFDecoder &dec);
	void onCmd_connect(AMFDecoder &dec);
	void onCmd_createStream(AMFDecoder &dec);

	void onCmd_publish(AMFDecoder &dec);
	void onCmd_deleteStream(AMFDecoder &dec);

	void onCmd_play(AMFDecoder &dec);
	void onCmd_play2(AMFDecoder &dec);
	void doPlay(AMFDecoder &dec);
    void doPlayResponse(const string &err,bool tryDelay);
	void onCmd_seek(AMFDecoder &dec);
	void onCmd_pause(AMFDecoder &dec);
	void setMetaData(AMFDecoder &dec);

	void onSendMedia(const RtmpPacket::Ptr &pkt);
	void onSendRawData(const char *pcRawData,int iSize) override{
        m_ui64TotalBytes += iSize;
		send(pcRawData, iSize);
	}
	void onSendRawData(const Buffer::Ptr &buffer,int flags) override{
        m_ui64TotalBytes += buffer->size();
        _sock->send(buffer,flags);
	}
	void onRtmpChunk(RtmpPacket &chunkData) override;

	template<typename first, typename second>
	inline void sendReply(const char *str, const first &reply, const second &status) {
		AMFEncoder invoke;
		invoke << str << m_dNowReqID << reply << status;
		sendResponse(MSG_CMD, invoke.data());
	}

    bool shutDown() override {
        InfoL << "kick out:" << m_mediaInfo.m_vhost << " " << m_mediaInfo.m_app << " " << m_mediaInfo.m_streamid;
        safeShutdown();
        return true;
    }

    void doDelay(int delaySec,const std::function<void()> &fun);
    std::function<void()> m_delayTask;
    uint32_t m_iTaskTimeLine = 0;

};

} /* namespace Rtmp */
} /* namespace ZL */

#endif /* SRC_RTMP_RTMPSESSION_H_ */
