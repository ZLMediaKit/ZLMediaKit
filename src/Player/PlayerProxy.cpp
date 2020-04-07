/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Common/config.h"
#include "PlayerProxy.h"
#include "Util/mini.h"
#include "Util/MD5.h"
#include "Util/logger.h"
#include "Extension/AAC.h"

using namespace toolkit;

namespace mediakit {

static uint8_t s_mute_adts[] = {0xff, 0xf1, 0x6c, 0x40, 0x2d, 0x3f, 0xfc, 0x00, 0xe0, 0x34, 0x20, 0xad, 0xf2, 0x3f, 0xb5, 0xdd,
                                0x73, 0xac, 0xbd, 0xca, 0xd7, 0x7d, 0x4a, 0x13, 0x2d, 0x2e, 0xa2, 0x62, 0x02, 0x70, 0x3c, 0x1c,
                                0xc5, 0x63, 0x55, 0x69, 0x94, 0xb5, 0x8d, 0x70, 0xd7, 0x24, 0x6a, 0x9e, 0x2e, 0x86, 0x24, 0xea,
                                0x4f, 0xd4, 0xf8, 0x10, 0x53, 0xa5, 0x4a, 0xb2, 0x9a, 0xf0, 0xa1, 0x4f, 0x2f, 0x66, 0xf9, 0xd3,
                                0x8c, 0xa6, 0x97, 0xd5, 0x84, 0xac, 0x09, 0x25, 0x98, 0x0b, 0x1d, 0x77, 0x04, 0xb8, 0x55, 0x49,
                                0x85, 0x27, 0x06, 0x23, 0x58, 0xcb, 0x22, 0xc3, 0x20, 0x3a, 0x12, 0x09, 0x48, 0x24, 0x86, 0x76,
                                0x95, 0xe3, 0x45, 0x61, 0x43, 0x06, 0x6b, 0x4a, 0x61, 0x14, 0x24, 0xa9, 0x16, 0xe0, 0x97, 0x34,
                                0xb6, 0x58, 0xa4, 0x38, 0x34, 0x90, 0x19, 0x5d, 0x00, 0x19, 0x4a, 0xc2, 0x80, 0x4b, 0xdc, 0xb7,
                                0x00, 0x18, 0x12, 0x3d, 0xd9, 0x93, 0xee, 0x74, 0x13, 0x95, 0xad, 0x0b, 0x59, 0x51, 0x0e, 0x99,
                                0xdf, 0x49, 0x98, 0xde, 0xa9, 0x48, 0x4b, 0xa5, 0xfb, 0xe8, 0x79, 0xc9, 0xe2, 0xd9, 0x60, 0xa5,
                                0xbe, 0x74, 0xa6, 0x6b, 0x72, 0x0e, 0xe3, 0x7b, 0x28, 0xb3, 0x0e, 0x52, 0xcc, 0xf6, 0x3d, 0x39,
                                0xb7, 0x7e, 0xbb, 0xf0, 0xc8, 0xce, 0x5c, 0x72, 0xb2, 0x89, 0x60, 0x33, 0x7b, 0xc5, 0xda, 0x49,
                                0x1a, 0xda, 0x33, 0xba, 0x97, 0x9e, 0xa8, 0x1b, 0x6d, 0x5a, 0x77, 0xb6, 0xf1, 0x69, 0x5a, 0xd1,
                                0xbd, 0x84, 0xd5, 0x4e, 0x58, 0xa8, 0x5e, 0x8a, 0xa0, 0xc2, 0xc9, 0x22, 0xd9, 0xa5, 0x53, 0x11,
                                0x18, 0xc8, 0x3a, 0x39, 0xcf, 0x3f, 0x57, 0xb6, 0x45, 0x19, 0x1e, 0x8a, 0x71, 0xa4, 0x46, 0x27,
                                0x9e, 0xe9, 0xa4, 0x86, 0xdd, 0x14, 0xd9, 0x4d, 0xe3, 0x71, 0xe3, 0x26, 0xda, 0xaa, 0x17, 0xb4,
                                0xac, 0xe1, 0x09, 0xc1, 0x0d, 0x75, 0xba, 0x53, 0x0a, 0x37, 0x8b, 0xac, 0x37, 0x39, 0x41, 0x27,
                                0x6a, 0xf0, 0xe9, 0xb4, 0xc2, 0xac, 0xb0, 0x39, 0x73, 0x17, 0x64, 0x95, 0xf4, 0xdc, 0x33, 0xbb,
                                0x84, 0x94, 0x3e, 0xf8, 0x65, 0x71, 0x60, 0x7b, 0xd4, 0x5f, 0x27, 0x79, 0x95, 0x6a, 0xba, 0x76,
                                0xa6, 0xa5, 0x9a, 0xec, 0xae, 0x55, 0x3a, 0x27, 0x48, 0x23, 0xcf, 0x5c, 0x4d, 0xbc, 0x0b, 0x35,
                                0x5c, 0xa7, 0x17, 0xcf, 0x34, 0x57, 0xc9, 0x58, 0xc5, 0x20, 0x09, 0xee, 0xa5, 0xf2, 0x9c, 0x6c,
                                0x39, 0x1a, 0x77, 0x92, 0x9b, 0xff, 0xc6, 0xae, 0xf8, 0x36, 0xba, 0xa8, 0xaa, 0x6b, 0x1e, 0x8c,
                                0xc5, 0x97, 0x39, 0x6a, 0xb8, 0xa2, 0x55, 0xa8, 0xf8};
#define MUTE_ADTS_DATA s_mute_adts
#define MUTE_ADTS_DATA_LEN sizeof(s_mute_adts)
#define MUTE_ADTS_DATA_MS 130

PlayerProxy::PlayerProxy(const string &strVhost,
                         const string &strApp,
                         const string &strSrc,
                         bool bEnableRtsp,
                         bool bEnableRtmp,
                         bool bEnableHls,
                         bool bEnableMp4,
                         int iRetryCount,
                         const EventPoller::Ptr &poller) : MediaPlayer(poller){
    _strVhost = strVhost;
    _strApp = strApp;
    _strSrc = strSrc;
    _bEnableRtsp = bEnableRtsp;
    _bEnableRtmp = bEnableRtmp;
    _bEnableHls = bEnableHls;
    _bEnableMp4 = bEnableMp4;
    _iRetryCount = iRetryCount;
}

void PlayerProxy::setPlayCallbackOnce(const function<void(const SockException &ex)> &cb){
    _playCB = cb;
}

void PlayerProxy::setOnClose(const function<void()> &cb){
    _onClose = cb;
}

void PlayerProxy::play(const string &strUrlTmp) {
    weak_ptr<PlayerProxy> weakSelf = shared_from_this();
    std::shared_ptr<int> piFailedCnt(new int(0)); //连续播放失败次数
    setOnPlayResult([weakSelf,strUrlTmp,piFailedCnt](const SockException &err) {
        auto strongSelf = weakSelf.lock();
        if(!strongSelf) {
            return;
        }

        if(strongSelf->_playCB) {
            strongSelf->_playCB(err);
            strongSelf->_playCB = nullptr;
        }

        if(!err) {
            // 播放成功
            *piFailedCnt = 0;//连续播放失败次数清0
            strongSelf->onPlaySuccess();
        }else if(*piFailedCnt < strongSelf->_iRetryCount || strongSelf->_iRetryCount < 0) {
            // 播放失败，延时重试播放
            strongSelf->rePlay(strUrlTmp,(*piFailedCnt)++);
        }
    });
    setOnShutdown([weakSelf,strUrlTmp,piFailedCnt](const SockException &err) {
        auto strongSelf = weakSelf.lock();
        if(!strongSelf) {
            return;
        }
        if(strongSelf->_mediaMuxer) {
            auto tracks = strongSelf->getTracks(false);
            for (auto & track : tracks){
                track->delDelegate(strongSelf->_mediaMuxer.get());
            }

            GET_CONFIG(bool,resetWhenRePlay,General::kResetWhenRePlay);
            if (resetWhenRePlay) {
                strongSelf->_mediaMuxer.reset();
            } else {
                strongSelf->_mediaMuxer->resetTracks();
            }
        }
        //播放异常中断，延时重试播放
        if(*piFailedCnt < strongSelf->_iRetryCount || strongSelf->_iRetryCount < 0) {
            strongSelf->rePlay(strUrlTmp,(*piFailedCnt)++);
        }
    });
    MediaPlayer::play(strUrlTmp);

    MediaSource::Ptr mediaSource;
    if(dynamic_pointer_cast<RtspPlayer>(_delegate)){
        //rtsp拉流
        GET_CONFIG(bool,directProxy,Rtsp::kDirectProxy);
        if(directProxy && _bEnableRtsp){
            mediaSource = std::make_shared<RtspMediaSource>(_strVhost,_strApp,_strSrc);
        }
    } else if(dynamic_pointer_cast<RtmpPlayer>(_delegate)){
        //rtmp拉流
        if(_bEnableRtmp){
            mediaSource = std::make_shared<RtmpMediaSource>(_strVhost,_strApp,_strSrc);
        }
    }
    if(mediaSource){
        setMediaSouce(mediaSource);
        mediaSource->setListener(shared_from_this());
    }
}

PlayerProxy::~PlayerProxy() {
    _timer.reset();
}
void PlayerProxy::rePlay(const string &strUrl,int iFailedCnt){
    auto iDelay = MAX(2 * 1000, MIN(iFailedCnt * 3000, 60*1000));
    weak_ptr<PlayerProxy> weakSelf = shared_from_this();
    _timer = std::make_shared<Timer>(iDelay / 1000.0f,[weakSelf,strUrl,iFailedCnt]() {
        //播放失败次数越多，则延时越长
        auto strongPlayer = weakSelf.lock();
        if(!strongPlayer) {
            return false;
        }
        WarnL << "重试播放[" << iFailedCnt << "]:"  << strUrl;
        strongPlayer->MediaPlayer::play(strUrl);
        return false;
    }, getPoller());
}

bool PlayerProxy::close(MediaSource &sender,bool force) {
    if(!force && totalReaderCount()){
        return false;
    }

    //通知其停止推流
    weak_ptr<PlayerProxy> weakSlef = dynamic_pointer_cast<PlayerProxy>(shared_from_this());
    getPoller()->async_first([weakSlef]() {
        auto stronSelf = weakSlef.lock();
        if (stronSelf) {
            stronSelf->_mediaMuxer.reset();
            stronSelf->setMediaSouce(nullptr);
            stronSelf->teardown();
            if(stronSelf->_onClose){
                stronSelf->_onClose();
            }
        }
    });
    WarnL << sender.getSchema() << "/" << sender.getVhost() << "/" << sender.getApp() << "/" << sender.getId() << " " << force;
    return true;
}

int PlayerProxy::totalReaderCount(){
    return (_mediaMuxer ? _mediaMuxer->totalReaderCount() : 0) + (_pMediaSrc ? _pMediaSrc->readerCount() : 0);
}

int PlayerProxy::totalReaderCount(MediaSource &sender) {
    return totalReaderCount();
}

class MuteAudioMaker : public FrameDispatcher{
public:
    typedef std::shared_ptr<MuteAudioMaker> Ptr;

    MuteAudioMaker(){};
    virtual ~MuteAudioMaker(){}
    void inputFrame(const Frame::Ptr &frame) override {
        if(frame->getTrackType() == TrackVideo){
            auto iAudioIndex = frame->dts() / MUTE_ADTS_DATA_MS;
            if(_iAudioIndex != iAudioIndex){
                _iAudioIndex = iAudioIndex;
                auto aacFrame = std::make_shared<AACFrameCacheAble>((char *)MUTE_ADTS_DATA, MUTE_ADTS_DATA_LEN, _iAudioIndex * MUTE_ADTS_DATA_MS, 0);
                FrameDispatcher::inputFrame(aacFrame);
            }
        }
    }

private:
    class AACFrameCacheAble : public AACFrameNoCacheAble{
    public:
        template <typename ... ARGS>
        AACFrameCacheAble(ARGS && ...args) : AACFrameNoCacheAble(std::forward<ARGS>(args)...){};
        virtual ~AACFrameCacheAble() = default;

        bool cacheAble() const override {
            return true;
        }
    };
private:
    int _iAudioIndex = 0;
};

void PlayerProxy::onPlaySuccess() {
    GET_CONFIG(bool,resetWhenRePlay,General::kResetWhenRePlay);
    if (dynamic_pointer_cast<RtspMediaSource>(_pMediaSrc)) {
        //rtsp拉流代理
        if (resetWhenRePlay || !_mediaMuxer) {
            _mediaMuxer.reset(new MultiMediaSourceMuxer(_strVhost, _strApp, _strSrc, getDuration(), false, _bEnableRtmp, _bEnableHls, _bEnableMp4));
        }
    } else if (dynamic_pointer_cast<RtmpMediaSource>(_pMediaSrc)) {
        //rtmp拉流代理
        if (resetWhenRePlay || !_mediaMuxer) {
            _mediaMuxer.reset(new MultiMediaSourceMuxer(_strVhost, _strApp, _strSrc, getDuration(), _bEnableRtsp, false, _bEnableHls, _bEnableMp4));
        }
    } else {
        //其他拉流代理
        if (resetWhenRePlay || !_mediaMuxer) {
            _mediaMuxer.reset(new MultiMediaSourceMuxer(_strVhost, _strApp, _strSrc, getDuration(), _bEnableRtsp, _bEnableRtmp, _bEnableHls, _bEnableMp4));
        }
    }
    _mediaMuxer->setMediaListener(shared_from_this());

    auto videoTrack = getTrack(TrackVideo,false);
    if(videoTrack){
        //添加视频
        _mediaMuxer->addTrack(videoTrack);
        //视频数据写入_mediaMuxer
        videoTrack->addDelegate(_mediaMuxer);
    }

    //是否添加静音音频
    GET_CONFIG(bool,addMuteAudio,General::kAddMuteAudio);

    auto audioTrack = getTrack(TrackAudio, false);
    if(audioTrack){
        //添加音频
        _mediaMuxer->addTrack(audioTrack);
        //音频数据写入_mediaMuxer
        audioTrack->addDelegate(_mediaMuxer);
    }else if(addMuteAudio && videoTrack){
        //没有音频信息，产生一个静音音频
        MuteAudioMaker::Ptr audioMaker = std::make_shared<MuteAudioMaker>();
        //videoTrack把数据写入MuteAudioMaker
        videoTrack->addDelegate(audioMaker);
        //添加一个静音Track至_mediaMuxer
        _mediaMuxer->addTrack(std::make_shared<AACTrack>());
        //MuteAudioMaker生成静音音频然后写入_mediaMuxer；
        audioMaker->addDelegate(_mediaMuxer);
    }

    //添加完毕所有track，防止单track情况下最大等待3秒
    _mediaMuxer->addTrackCompleted();

    if(_pMediaSrc){
        _pMediaSrc->setTrackSource(_mediaMuxer);
    }
}


} /* namespace mediakit */
