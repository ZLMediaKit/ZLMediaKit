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

    class TsDemuxer : public MediaSinkInterface, public TrackSource, public std::enable_shared_from_this<TsDemuxer> {
    public:
        TsDemuxer() = default;

        ~TsDemuxer() override { _timer = nullptr; }

        void start(const EventPoller::Ptr &poller, TrackListener *listener);

        bool inputFrame(const Frame::Ptr &frame) override;

        bool addTrack(const Track::Ptr &track) override {
            return _delegate.addTrack(track);
        }

        void addTrackCompleted() override {
            _delegate.addTrackCompleted();
        }

        void resetTracks() override {
            ((MediaSink &) _delegate).resetTracks();
        }

        vector<Track::Ptr> getTracks(bool ready = true) const override {
            return _delegate.getTracks(ready);
        }

    private:
        void onTick();

        int64_t getBufferMS();

        int64_t getPlayPosition();

        void setPlayPosition(int64_t pos);

    private:
        int64_t _ticker_offset = 0;
        Ticker _ticker;
        Stamp _stamp[2];
        Timer::Ptr _timer;
        MediaSinkDelegate _delegate;
        multimap<int64_t, Frame::Ptr> _frame_cache;
    };

//    class TsPlayerImp : public PlayerImp<TsPlayer, PlayerBase>, private TrackListener {
//    public:
//        typedef std::shared_ptr<TsPlayerImp> Ptr;
//        TsPlayerImp(const EventPoller::Ptr &poller = nullptr);
//        ~TsPlayerImp() override = default;
//
//    private:
//        //// TsPlayer override////
//        void onPacket(const char *data, size_t len) override{
//
//            if (!_decoder) {
//                _decoder = DecoderImp::createDecoder(DecoderImp::decoder_ts, _demuxer.get());
//            }
//
//            if (_decoder && _demuxer) {
//                _decoder->input((uint8_t *) data, len);
//            }
//
//        }
//        //// PlayerBase override////
//        void onPlayResult(const SockException &ex) override{
//            WarnL << ex.getErrCode() << " " << ex.what();
//            if (ex) {
//                PlayerImp<TsPlayer, PlayerBase>::onPlayResult(ex);
//            } else {
//                auto demuxer = std::make_shared<HlsDemuxer>();
//                demuxer->start(getPoller(), this);
//                _demuxer = std::move(demuxer);
//            }
//        }
//
//        bool inputFrame(const Frame::Ptr &frame) override{
//            //计算相对时间戳
//            int64_t dts, pts;
////            _stamp[frame->getTrackType()].revise(frame->dts(), frame->pts(), dts, pts);
////            WarnL << getTrackString(frame->getTrackType()) << "当前dts/pts[" << frame->dts() << "/" << frame->pts() << "]["<<dts<<"/"<<pts<< "]最后dts[" << last_dts << "]";
//            // 加速trackReady速度, 避免网络抖动时由于超时而放弃video track, 同时加速首屏显示速度
//            if(!MediaSink::isTrackReady()){
//                MediaSink::inputFrame(Frame::getCacheAbleFrame(frame));
//                return true;
//            }
//            // 根据时间戳缓存frame
////            _frame_cache.emplace(dts, Frame::getCacheAbleFrame(frame));
//            _frame_cache.emplace(frame->dts(), Frame::getCacheAbleFrame(frame));
////            WarnL << "缓存时间[" << getBufferMS() << "]";
//            if (getBufferMS() > 180 * 1000) {
//                // 缓存限制最大180秒, 超过180秒强制消费60秒(减少延时或内存占用)
//                // 拉取http-ts流时, 部分的上游输出并不是平滑的, 也就是可能一秒输出几mb数据, 然后会间隔几秒甚至十来秒再次输出,
//                // 这种情况特别会出现在跨国家地域的拉流情况中, 一部分是上游源输出的问题, 一部分是由于国际出口网络或者isp导致的失速问题.
//                // 对于输出http-ts/rtmp/rtsp等流媒体影响并不大, 但是如果输出hls, 那么就会导致m3u8文件中更新segment并不是平滑的,
//                // 而是会出现跳段的现象.
//                // 所以如果主要是输出hls, 那么最好是不强制消费, 只通过tick周期性平滑消费帧.因为输出是hls, 等于一直在进行消费.
//                // 因此不会存在内存占用过大的问题.
//                // 所以比较好的解决方法应该是判断是否存在hls输出, 如果存在hls输出则跳过这个限制缓存时间的逻辑.否则限制缓存时间为30秒,
//                // 超过缓存时间则强制消费15秒. 但是目前并没有找到一个低成本的判断是否存在hls输出的方法, 所以只有增加缓存时间.
//                while (getBufferMS() > 60 * 1000) {
//                    MediaSink::inputFrame(_frame_cache.begin()->second);
//                    _frame_cache.erase(_frame_cache.begin());
//                }
//                //接着播放缓存中最早的帧
//                setPlayPosition(_frame_cache.begin()->first);
//            }
//            return true;
//        }
//
//        void onShutdown(const SockException &ex) override {
//            PlayerImp<TsPlayer, PlayerBase>::onShutdown(ex);
//            _demuxer = nullptr;
//        }
//
//        void onTick(){
//            auto it = _frame_cache.begin();
//            while (it != _frame_cache.end()) {
//                if (it->first > getPlayPosition()) {
//                    //这些帧还未到时间播放
//                    break;
//                }
//                if (getBufferMS() < 3 * 1000) {
//                    //缓存小于3秒,那么降低定时器消费速度(让剩余的数据在3秒后消费完毕)
//                    //目的是为了防止定时器长时间干等后，数据瞬间消费完毕
//                    setPlayPosition(_frame_cache.begin()->first);
//                }
//                //消费掉已经到期的帧
//                MediaSink::inputFrame(it->second);
//                it = _frame_cache.erase(it);
//            }
//        }
//        //// TrackListener override////
//        bool addTrack(const Track::Ptr &track) override { return true; };
//
//        void addTrackCompleted() override{
//            PlayerImp<TsPlayer, PlayerBase>::onPlayResult(SockException(Err_success, "play hls success"));
//        };
//
//    private:
//        DecoderImp::Ptr _decoder;
//        MediaSinkInterface::Ptr _demuxer;
//    };
//
    class TsPlayerImp : public PlayerImp<TsPlayer, PlayerBase>, private TrackListener {
    public:
        typedef std::shared_ptr<TsPlayerImp> Ptr;

        TsPlayerImp(const EventPoller::Ptr &poller = nullptr);

        ~TsPlayerImp() override = default;

    private:
        //// HlsPlayer override////
        void onPacket(const char *data, size_t len) override;

    private:
        //// PlayerBase override////
        void onPlayResult(const SockException &ex) override;

        vector<Track::Ptr> getTracks(bool ready = true) const override;

        void onShutdown(const SockException &ex) override;

    private:
        //// TrackListener override////
        bool addTrack(const Track::Ptr &track) override { return true; };

        void addTrackCompleted() override;

    private:
        DecoderImp::Ptr _decoder;
        MediaSinkInterface::Ptr _demuxer;
    };
}//namespace mediakit
#endif //HTTP_TSPLAYERIMP_H
