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
