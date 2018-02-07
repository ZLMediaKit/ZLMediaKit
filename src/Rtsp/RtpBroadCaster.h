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

#ifndef SRC_RTSP_RTPBROADCASTER_H_
#define SRC_RTSP_RTPBROADCASTER_H_


#include <mutex>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include "Common/config.h"
#include "RtspMediaSource.h"
#include "Util/mini.h"
#include "Network/Socket.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Network;

namespace ZL {
namespace Rtsp {

class MultiCastAddressMaker
{
public:
	static MultiCastAddressMaker &Instance(){
		static MultiCastAddressMaker instance;
		return instance;
	}
	static bool isMultiCastAddress(uint32_t iAddr){
		static uint32_t addrMin = mINI::Instance()[Config::MultiCast::kAddrMin].as<uint32_t>();
		static uint32_t addrMax = mINI::Instance()[Config::MultiCast::kAddrMax].as<uint32_t>();
		return iAddr >= addrMin && iAddr <= addrMax;
	}
	static string toString(uint32_t iAddr){
		iAddr = htonl(iAddr);
		return ::inet_ntoa((struct in_addr &)(iAddr));
	}
	virtual ~MultiCastAddressMaker(){}
	std::shared_ptr<uint32_t> obtain(uint32_t iTry = 10);
private:
	MultiCastAddressMaker(){};
	void release(uint32_t iAddr);
	uint32_t m_iAddr = 0;
	recursive_mutex m_mtx;
	unordered_set<uint32_t> m_setBadAddr;
};
class RtpBroadCaster {
public:
	typedef std::shared_ptr<RtpBroadCaster> Ptr;
	typedef function<void()> onDetach;
	virtual ~RtpBroadCaster();
	static Ptr get(const string &strLocalIp,const string &strVhost,const string &strApp,const string &strStream);
	void setDetachCB(void *listener,const onDetach &cb);
	uint16_t getPort(int iTrackId);
	string getIP();
private:
	static recursive_mutex g_mtx;
	static unordered_map<string , weak_ptr<RtpBroadCaster> > g_mapBroadCaster;
	static Ptr make(const string &strLocalIp,const string &strVhost,const string &strApp,const string &strStream);

	std::shared_ptr<uint32_t> m_multiAddr;
	recursive_mutex m_mtx;
	unordered_map<void * , onDetach > m_mapDetach;
	RtspMediaSource::RingType::RingReader::Ptr m_pReader;
	Socket::Ptr m_apUdpSock[2];
	struct sockaddr_in m_aPeerUdpAddr[2];

	RtpBroadCaster(const string &strLocalIp,const string &strVhost,const string &strApp,const string &strStream);

};

} /* namespace Rtsp */
} /* namespace ZL */

#endif /* SRC_RTSP_RTPBROADCASTER_H_ */
