//
// Created by alex on 2021/7/5.
//

#ifndef HTTP_TSPLAYERIMP_H
#define HTTP_TSPLAYERIMP_H

#include <unordered_set>
#include "Util/util.h"
#include "Poller/Timer.h"
#include "Http/HttpDownloader.h"
#include "Player/MediaPlayer.h"
#include "Rtp/Decoder.h"
#include "Rtp/TSDecoder.h"
#include "TsPlayer.h"

using namespace toolkit;
namespace mediakit {
    class TsPlayerImp : public PlayerImp<TsPlayer, PlayerBase>, public MediaSink {
    public:
        typedef std::shared_ptr<TsPlayerImp> Ptr;
        TsPlayerImp(const EventPoller::Ptr &poller) : PlayerImp<TsPlayer, PlayerBase>(poller) {}
        ~TsPlayerImp() override{
            DebugL << endl;
        }

    private:
        void onPacket(const char *data, size_t len) override{
            if (!_decoder) {
                _decoder = DecoderImp::createDecoder(DecoderImp::decoder_ts, this);
            }
            if (_decoder) {
                _decoder->input((uint8_t *) data, len);
            }
        }
        void onAllTrackReady() override{
            PlayerImp<TsPlayer, PlayerBase>::onPlayResult(SockException(Err_success, "play ts success"));
        }

        void onPlayResult(const SockException &ex) override{
            WarnL << ex.getErrCode() << " " << ex.what();
            if (ex) {
                PlayerImp<TsPlayer, PlayerBase>::onPlayResult(ex);
            } else {
                _frame_cache.clear();
                _stamp[TrackAudio].setRelativeStamp(0);
                _stamp[TrackVideo].setRelativeStamp(0);
                _stamp[TrackAudio].syncTo(_stamp[TrackVideo]);
                setPlayPosition(0);
                weak_ptr<TsPlayerImp> weakSelf = dynamic_pointer_cast<TsPlayerImp>(shared_from_this());
                //每50毫秒执行一次
                _timer = std::make_shared<Timer>(0.05f, [weakSelf]() {
                    auto strongSelf = weakSelf.lock();
                    if (!strongSelf) {
                        return false;
                    }
                    strongSelf->onTick();
                    return true;
                }, getPoller());
            }
        }

        vector<Track::Ptr> getTracks(bool trackReady = true) const override{
            return MediaSink::getTracks(trackReady);
        }

        void inputFrame(const Frame::Ptr &frame) override{
            //计算相对时间戳
            int64_t dts, pts;
//        WarnL << "当前dts/pts[" << frame->dts() << "/" << frame->pts() << "] 最后dts[" << last_dts << "]";
            // 放弃掉小于缓存中最后一个帧dts的其他帧
//            if (last_dts >= frame->dts() && isReconnect) {
//                return;
//            }
            if(isReconnect){
                WarnL << getTrackString(frame->getTrackType()) << "当前dts/pts[" << frame->dts() << "/" << frame->pts() << "] 最后dts[" << last_dts << "]";
            }
            last_dts = frame->dts();
            isReconnect = false;
            _stamp[frame->getTrackType()].revise(frame->dts(), frame->pts(), dts, pts);
            if(!MediaSink::isTrackReady()){
                MediaSink::inputFrame(Frame::getCacheAbleFrame(frame));
                return;
            }
            //根据时间戳缓存frame
            _frame_cache.emplace(dts, Frame::getCacheAbleFrame(frame));

//        WarnL << "缓存时间[" << getBufferMS() << "]";
//            if (getBufferMS() > 30 * 1000) {
//                //缓存超过30秒，强制消费至15秒(减少延时或内存占用)
//                while (getBufferMS() > 15 * 1000) {
//                    MediaSink::inputFrame(_frame_cache.begin()->second);
//                    _frame_cache.erase(_frame_cache.begin());
//                }
//                //接着播放缓存中最早的帧
//                setPlayPosition(_frame_cache.begin()->first);
//            }
        }

        void onShutdown(const SockException &ex) override {
            PlayerImp<TsPlayer, PlayerBase>::onShutdown(ex);
            _timer = nullptr;
        }

        void onTick(){
            auto it = _frame_cache.begin();
            while (it != _frame_cache.end()) {
                if (it->first > getPlayPosition()) {
                    //这些帧还未到时间播放
                    break;
                }
                if (getBufferMS() < 3 * 1000) {
                    //缓存小于3秒,那么降低定时器消费速度(让剩余的数据在3秒后消费完毕)
                    //目的是为了防止定时器长时间干等后，数据瞬间消费完毕
                    setPlayPosition(_frame_cache.begin()->first);
                }
                //消费掉已经到期的帧
                MediaSink::inputFrame(it->second);
                it = _frame_cache.erase(it);
            }
        }
        int64_t getPlayPosition(){
            return _ticker.elapsedTime() + _ticker_offset;
        }

        void setPlayPosition(int64_t pos) {
            _ticker.resetTime();
            _ticker_offset = pos;
        }

        int64_t getBufferMS(){
            if (_frame_cache.empty()) {
                return 0;
            }
            return _frame_cache.rbegin()->first - _frame_cache.begin()->first;
        }

    private:
        int64_t _ticker_offset = 0;
        int64_t last_dts = -1;
        Ticker _ticker;
        Stamp _stamp[2];
        Timer::Ptr _timer;
        DecoderImp::Ptr _decoder;
        multimap<int64_t, Frame::Ptr> _frame_cache;
    };

}//namespace mediakit
#endif //HTTP_TSPLAYERIMP_H
