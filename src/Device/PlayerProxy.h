/*
 * PlyerProxy.h
 *
 *  Created on: 2016年12月6日
 *      Author: xzl
 */

#ifndef SRC_DEVICE_PLAYERPROXY_H_
#define SRC_DEVICE_PLAYERPROXY_H_

#include "Device.h"
#include <memory>
#include "Player/MediaPlayer.h"
#include "Util/TimeTicker.h"

using namespace std;
using namespace ZL::Player;

namespace ZL {
namespace DEV {

class PlayerProxy : public std::enable_shared_from_this<PlayerProxy>{
public:
	typedef std::shared_ptr<PlayerProxy> Ptr;
	PlayerProxy(const char *strApp, const char *strSrc);
	void play(const char* strUrl, const char *strUser = "", const char *strPwd = "",PlayerBase::eRtpType eType = PlayerBase::RTP_TCP,uint32_t iSecond = 0);
	virtual ~PlayerProxy();
	void setOnExpired(const function<void()> &cb){
		onExpired = cb;
	}
private :
	MediaPlayer::Ptr m_pPlayer;
	DevChannel::Ptr m_pChn;
	Ticker m_aliveTicker;
	uint32_t m_aliveSecond = 0;
	function<void()> onExpired;
	string m_strApp;
	string m_strSrc;
	void initMedia();
	void rePlay(const string &strUrl, const string &strUser, const string &strPwd, PlayerBase::eRtpType eType,uint64_t iFailedCnt);
	void checkExpired();
	void expired();
};

} /* namespace Player */
} /* namespace ZL */

#endif /* SRC_DEVICE_PLAYERPROXY_H_ */
