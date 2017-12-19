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

#include "player.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Util/onceToken.h"
#include "Thread/ThreadPool.h"
#include "Poller/EventPoller.h"
#include "Player/MediaPlayer.h"
#include "H264/H264Parser.h"
#include "cleaner.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Thread;
using namespace ZL::Player;
using namespace ZL::Rtmp;
using namespace ZL::Rtsp;

static recursive_mutex s_mtxMapPlayer;
static unordered_map<void *, MediaPlayer::Ptr> s_mapPlayer;

static onceToken s_token([](){
	cleaner::Instance().push_front([](){
		lock_guard<recursive_mutex> lck(s_mtxMapPlayer);
		s_mapPlayer.clear();
		DebugL << "clear player" << endl;
	});
},nullptr);


////////////////////////rtsp player/////////////////////////////////////////
#define getPlayer(ctx) \
		MediaPlayer::Ptr player;\
		{\
			lock_guard<recursive_mutex> lck(s_mtxMapPlayer);\
			auto it = s_mapPlayer.find(ctx);\
			if(it != s_mapPlayer.end()){\
				player = it->second;\
			}\
		}
API_EXPORT PlayerContext API_CALL createPlayer() {
	lock_guard<recursive_mutex> lck(s_mtxMapPlayer);
	MediaPlayer::Ptr ret(new MediaPlayer());
	s_mapPlayer.emplace(ret.get(), ret);
	if(s_mapPlayer.size() > 16){
		FatalL << s_mapPlayer.size();
	}
	return ret.get();
}
API_EXPORT void API_CALL releasePlayer(PlayerContext ctx) {
	getPlayer(ctx);
	if (!player) {
		return;
	}
	player_setOnGetAudio(ctx, nullptr, nullptr);
	player_setOnGetVideo(ctx, nullptr, nullptr);
	player_setOnPlayResult(ctx, nullptr, nullptr);
	player_setOnShutdown(ctx, nullptr, nullptr);
	lock_guard<recursive_mutex> lck(s_mtxMapPlayer);
	s_mapPlayer.erase(ctx);

	ASYNC_TRACE([player]() {
		lock_guard<recursive_mutex> lck(s_mtxMapPlayer);
		player->teardown();
	});
}

API_EXPORT void API_CALL player_setOptionInt(PlayerContext ctx,const char* key,int val){
	string keyTmp(key);
	ASYNC_TRACE([ctx,keyTmp,val](){
		getPlayer(ctx);
		if (!player) {
			return;
		}
		(*player)[keyTmp] = val;
	});
}
API_EXPORT void API_CALL player_setOptionString(PlayerContext ctx,const char* key,const char *val){
	string keyTmp(key);
	string valTmp(val);
	ASYNC_TRACE([ctx,keyTmp,valTmp](){
		getPlayer(ctx);
		if (!player) {
			return;
		}
		(*player)[keyTmp] = valTmp;
	});
}
API_EXPORT void API_CALL player_play(PlayerContext ctx, const char* url) {
	string urlTmp(url);
	ASYNC_TRACE([ctx,urlTmp](){
		getPlayer(ctx);
		if (!player) {
			return;
		}
		player->play(urlTmp.data());
	});
}

API_EXPORT void API_CALL player_pause(PlayerContext ctx, int pause) {
	ASYNC_TRACE([ctx,pause](){
		getPlayer(ctx);
		if (!player) {
			return;
		}
		player->pause(pause);
	});
}

API_EXPORT void API_CALL player_seekTo(PlayerContext ctx, float fProgress) {
	ASYNC_TRACE([ctx,fProgress]() {
		getPlayer(ctx);
		if (!player) {
			return;
		}
		return player->seekTo(fProgress);
	});
}
API_EXPORT void API_CALL player_setOnShutdown(PlayerContext ctx, player_onResult cb, void *userData) {
	getPlayer(ctx);
	if (!player) {
		return;
	}
	if(cb){
		SYNC_TRACE([&](){
			player->setOnShutdown([cb,userData](const SockException &ex) {
                cb(userData,ex.getErrCode(),ex.what());
			});
		});
	}else{
        SYNC_TRACE([&](){
            player->setOnShutdown(nullptr);
        });
	}
}

API_EXPORT void API_CALL player_setOnPlayResult(PlayerContext ctx, player_onResult cb, void *userData) {
	getPlayer(ctx);
	if (!player) {
		return;
	}
	if (cb) {
		SYNC_TRACE([&](){
			player->setOnPlayResult([cb,userData](const SockException &ex) {
                cb(userData,ex.getErrCode(),ex.what());
			});
		});
	} else {
        SYNC_TRACE([&](){
            player->setOnPlayResult(nullptr);
        });
	}
}

API_EXPORT void API_CALL player_setOnGetVideo(PlayerContext ctx, player_onGetH264 cb,
		void *userData) {
	getPlayer(ctx);
	if (!player) {
		return;
	}
	if (cb) {
		std::shared_ptr<H264Parser> pParser(new H264Parser());
		SYNC_TRACE([&](){
			player->setOnVideoCB([cb,userData,pParser](const H264Frame &frame) {
                pParser->inputH264(frame.data, frame.timeStamp);
                cb(userData, (void *)frame.data.data(), frame.data.size(), frame.timeStamp,pParser->getPts());
			});
		});
	} else {
        SYNC_TRACE([&](){
            player->setOnVideoCB(nullptr);
        });
	}
}

API_EXPORT void API_CALL player_setOnGetAudio(PlayerContext ctx, player_onGetAAC cb,
		void *userData) {
	getPlayer(ctx);
	if (!player) {
		return;
	}
	if (cb) {
		SYNC_TRACE([&](){
			player->setOnAudioCB([cb,userData](const AdtsFrame &frame) {
                cb(userData, (void *)frame.data, frame.aac_frame_length, frame.timeStamp);
			});
		});
	} else {
        SYNC_TRACE([&](){
            player->setOnAudioCB(nullptr);
        });
	}
}

API_EXPORT int API_CALL player_getVideoWidth(PlayerContext ctx) {
	getPlayer(ctx);
	if (!player) {
		return -1;
	}
	return player->getVideoWidth();
}

API_EXPORT int API_CALL player_getVideoHeight(PlayerContext ctx) {
	getPlayer(ctx);
	if (!player) {
		return -1;
	}
	return player->getVideoHeight();
}

API_EXPORT int API_CALL player_getVideoFps(PlayerContext ctx) {
	getPlayer(ctx);
	if (!player) {
		return -1;
	}
	return player->getVideoFps();
}

API_EXPORT int API_CALL player_getAudioSampleRate(PlayerContext ctx) {
	getPlayer(ctx);
	if (!player) {
		return -1;
	}
	return player->getAudioSampleRate();
}

API_EXPORT int API_CALL player_getAudioSampleBit(PlayerContext ctx) {
	getPlayer(ctx);
	if (!player) {
		return -1;
	}
	return player->getAudioSampleBit();
}

API_EXPORT int API_CALL player_getAudioChannel(PlayerContext ctx) {
	getPlayer(ctx);
	if (!player) {
		return -1;
	}
	return player->getAudioChannel();
}

API_EXPORT int API_CALL player_getH264PPS(PlayerContext ctx, char *buf, int bufsize) {
	getPlayer(ctx);
	if (!player) {
		return -1;
	}
	if (bufsize < (int) player->getPps().size() || player->getPps().empty()) {
		return -1;
	}
	memcpy(buf, player->getPps().data(), player->getPps().size());
	return player->getPps().size();
}
API_EXPORT int API_CALL player_getH264SPS(PlayerContext ctx, char *buf, int bufsize) {
	getPlayer(ctx);
	if (!player) {
		return -1;
	}
	if (bufsize < (int) player->getSps().size() || player->getSps().empty()) {
		return -1;
	}
	memcpy(buf, player->getSps().data(), player->getSps().size());
	return player->getSps().size();
}

API_EXPORT int API_CALL player_getAacCfg(PlayerContext ctx, char *buf, int bufsize) {
	getPlayer(ctx);
	if (!player) {
		return -1;
	}
	if (bufsize < (int) player->getAudioCfg().size()) {
		return -1;
	}
	memcpy(buf, player->getAudioCfg().data(), player->getAudioCfg().size());
	return player->getAudioCfg().size();
}

API_EXPORT int API_CALL player_containAudio(PlayerContext ctx) {
	getPlayer(ctx);
	if (!player) {
		return -1;
	}
	return player->containAudio();

}
API_EXPORT int API_CALL player_containVideo(PlayerContext ctx) {
	getPlayer(ctx);
	if (!player) {
		return -1;
	}
	return player->containVideo();
}

API_EXPORT int API_CALL player_isInited(PlayerContext ctx) {
	getPlayer(ctx);
	if (!player) {
		return -1;
	}
	return player->isInited();
}
API_EXPORT float API_CALL player_getDuration(PlayerContext ctx) {
	getPlayer(ctx);
	if (!player) {
		return -1;
	}
	return player->getDuration();
}

API_EXPORT float API_CALL player_getProgress(PlayerContext ctx) {
	getPlayer(ctx);
	if (!player) {
		return -1;
	}
	return player->getProgress();
}

API_EXPORT float API_CALL player_getLossRate(PlayerContext ctx, int trackId) {
	getPlayer(ctx);
	if (!player) {
		return -1;
	}
	return player->getRtpLossRate(trackId);
}

