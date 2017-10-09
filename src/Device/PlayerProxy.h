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

#ifndef SRC_DEVICE_PLAYERPROXY_H_
#define SRC_DEVICE_PLAYERPROXY_H_

#include <memory>
#include "Device.h"
#include "Player/MediaPlayer.h"
#include "Util/TimeTicker.h"

using namespace std;
using namespace ZL::Player;

namespace ZL {
namespace DEV {

class PlayerProxy :public MediaPlayer, public std::enable_shared_from_this<PlayerProxy>{
public:
	typedef std::shared_ptr<PlayerProxy> Ptr;
	//设置代理时间，0为永久，其他为代理秒数
	//设置方法：proxy[PlayerProxy::kAliveSecond] = 100;
	static const char kAliveSecond[];

	PlayerProxy(const char *strApp, const char *strSrc);
	virtual ~PlayerProxy();
	void play(const char* strUrl) override;
	void setOnExpired(const function<void()> &cb){
		onExpired = cb;
	}
private :
	DevChannel::Ptr m_pChn;
	Ticker m_aliveTicker;
	uint32_t m_aliveSecond = 0;
	function<void()> onExpired;
	string m_strApp;
	string m_strSrc;
	void initMedia();
	void rePlay(const string &strUrl,uint64_t iFailedCnt);
	void checkExpired();
	void expired();
};

} /* namespace Player */
} /* namespace ZL */

#endif /* SRC_DEVICE_PLAYERPROXY_H_ */
