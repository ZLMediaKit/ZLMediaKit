/*
 * MIT License
 *
 * Copyright (c) 2019 xiongziliang <771730766@qq.com>
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

#include "mk_media.h"
#include "Util/logger.h"
#include "Common/Device.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

class MediaHelper : public MediaSourceEvent , public std::enable_shared_from_this<MediaHelper> {
public:
    typedef std::shared_ptr<MediaHelper> Ptr;
    template<typename ...ArgsType>
    MediaHelper(ArgsType &&...args){
        _channel = std::make_shared<DevChannel>(std::forward<ArgsType>(args)...);
    }
    ~MediaHelper(){}

    void attachEvent(){
        _channel->setListener(shared_from_this());
    }

    DevChannel::Ptr &getChannel(){
        return _channel;
    }

    void setCallBack(on_mk_media_close cb, void *user_data){
        _cb = cb;
        _user_data = user_data;
    }
protected:
    // 通知其停止推流
    bool close(MediaSource &sender,bool force) override{
        if(!force && _channel->totalReaderCount()){
            //非强制关闭且正有人在观看该视频
            return false;
        }
        if(!_cb){
            //未设置回调，没法关闭
            WarnL << "请使用mk_media_set_on_close函数设置回调函数!";
            return false;
        }
        //请在回调中调用mk_media_release函数释放资源,否则MediaSource::close()操作不会生效
        _cb(_user_data);
        WarnL << "close media:" << sender.getSchema() << "/" << sender.getVhost() << "/" << sender.getApp() << "/" << sender.getId() << " " << force;
        return true;
    }

    // 观看总人数
    int totalReaderCount(MediaSource &sender) override{
        return _channel->totalReaderCount();
    }
private:
    DevChannel::Ptr _channel;
    on_mk_media_close _cb;
    void *_user_data;
};

API_EXPORT void API_CALL mk_media_set_on_close(mk_media ctx, on_mk_media_close cb, void *user_data){
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    (*obj)->setCallBack(cb,user_data);
}

API_EXPORT int API_CALL mk_media_total_reader_count(mk_media ctx){
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    return (*obj)->getChannel()->totalReaderCount();
}

API_EXPORT mk_media API_CALL mk_media_create(const char *vhost, const char *app, const char *stream, float duration, int hls_enabled, int mp4_enabled) {
    assert(vhost && app && stream);
    MediaHelper::Ptr *obj(new MediaHelper::Ptr(new MediaHelper(vhost, app, stream, duration, true, true, hls_enabled, mp4_enabled)));
    (*obj)->attachEvent();
    return (mk_media) obj;
}

API_EXPORT void API_CALL mk_media_release(mk_media ctx) {
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    delete obj;
}

API_EXPORT void API_CALL mk_media_init_h264(mk_media ctx, int width, int height, int frameRate) {
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    VideoInfo info;
    info.iFrameRate = frameRate;
    info.iWidth = width;
    info.iHeight = height;
    (*obj)->getChannel()->initVideo(info);
}

API_EXPORT void API_CALL mk_media_init_h265(mk_media ctx, int width, int height, int frameRate) {
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    VideoInfo info;
    info.iFrameRate = frameRate;
    info.iWidth = width;
    info.iHeight = height;
    (*obj)->getChannel()->initH265Video(info);
}

API_EXPORT void API_CALL mk_media_init_aac(mk_media ctx, int channel, int sample_bit, int sample_rate, int profile) {
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    AudioInfo info;
    info.iSampleRate = sample_rate;
    info.iChannel = channel;
    info.iSampleBit = sample_bit;
    info.iProfile = profile;
    (*obj)->getChannel()->initAudio(info);
}

API_EXPORT void API_CALL mk_media_init_complete(mk_media ctx){
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    (*obj)->getChannel()->addTrackCompleted();
}

API_EXPORT void API_CALL mk_media_input_h264(mk_media ctx, void *data, int len, uint32_t dts, uint32_t pts) {
    assert(ctx && data && len > 0);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    (*obj)->getChannel()->inputH264((char *) data, len, dts, pts);
}

API_EXPORT void API_CALL mk_media_input_h265(mk_media ctx, void *data, int len, uint32_t dts, uint32_t pts) {
    assert(ctx && data && len > 0);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    (*obj)->getChannel()->inputH265((char *) data, len, dts, pts);
}

API_EXPORT void API_CALL mk_media_input_aac(mk_media ctx, void *data, int len, uint32_t dts, int with_adts_header) {
    assert(ctx && data && len > 0);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    (*obj)->getChannel()->inputAAC((char *) data, len, dts, with_adts_header);
}

API_EXPORT void API_CALL mk_media_input_aac1(mk_media ctx, void *data, int len, uint32_t dts, void *adts) {
    assert(ctx && data && len > 0 && adts);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    (*obj)->getChannel()->inputAAC((char *) data, len, dts, (char *) adts);
}




