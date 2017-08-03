/*
 * PlyerProxy.h
 *
 *  Created on: 2016年12月6日
 *      Author: xzl
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
