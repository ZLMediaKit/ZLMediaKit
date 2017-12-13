/*
 * MediaPlayer.h
 *
 *  Created on: 2016年12月5日
 *      Author: xzl
 */

#ifndef SRC_PLAYER_MEDIAPLAYER_H_
#define SRC_PLAYER_MEDIAPLAYER_H_

#include <memory>
#include <string>
#include "Player.h"
#include "PlayerBase.h"
#include "Rtsp/RtspPlayer.h"
#include "Rtmp/RtmpPlayer.h"

using namespace std;
using namespace ZL::Rtsp;
using namespace ZL::Rtmp;

namespace ZL {
namespace Player {

class MediaPlayer : public PlayerImp<PlayerBase,PlayerBase> {
public:
	typedef std::shared_ptr<MediaPlayer> Ptr;

	MediaPlayer();
	virtual ~MediaPlayer();
	void play(const char* strUrl) override;
	void pause(bool bPause) override;
	void teardown() override;
private:
	string m_strPrefix;

};

} /* namespace Player */
} /* namespace ZL */

#endif /* SRC_PLAYER_MEDIAPLAYER_H_ */
