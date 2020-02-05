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

API_EXPORT mk_media API_CALL mk_media_create(const char *vhost, const char *app, const char *stream, float duration, int hls_enabled, int mp4_enabled) {
    assert(vhost && app && stream);
    DevChannel::Ptr *obj(new DevChannel::Ptr(new DevChannel(vhost, app, stream, duration, true, true, hls_enabled, mp4_enabled)));
    return (mk_media) obj;
}

API_EXPORT void API_CALL mk_media_release(mk_media ctx) {
    assert(ctx);
    DevChannel::Ptr *obj = (DevChannel::Ptr *) ctx;
    delete obj;
}

API_EXPORT void API_CALL mk_media_init_h264(mk_media ctx, int width, int height, int frameRate) {
    assert(ctx);
    DevChannel::Ptr *obj = (DevChannel::Ptr *) ctx;
    VideoInfo info;
    info.iFrameRate = frameRate;
    info.iWidth = width;
    info.iHeight = height;
    (*obj)->initVideo(info);
}

API_EXPORT void API_CALL mk_media_init_h265(mk_media ctx, int width, int height, int frameRate) {
    assert(ctx);
    DevChannel::Ptr *obj = (DevChannel::Ptr *) ctx;
    VideoInfo info;
    info.iFrameRate = frameRate;
    info.iWidth = width;
    info.iHeight = height;
    (*obj)->initH265Video(info);
}

API_EXPORT void API_CALL mk_media_init_aac(mk_media ctx, int channel, int sample_bit, int sample_rate, int profile) {
    assert(ctx);
    DevChannel::Ptr *obj = (DevChannel::Ptr *) ctx;
    AudioInfo info;
    info.iSampleRate = sample_rate;
    info.iChannel = channel;
    info.iSampleBit = sample_bit;
    info.iProfile = profile;
    (*obj)->initAudio(info);
}

API_EXPORT void API_CALL mk_media_input_h264(mk_media ctx, void *data, int len, uint32_t dts, uint32_t pts) {
    assert(ctx && data && len > 0);
    DevChannel::Ptr *obj = (DevChannel::Ptr *) ctx;
    (*obj)->inputH264((char *) data, len, dts, pts);
}

API_EXPORT void API_CALL mk_media_input_h265(mk_media ctx, void *data, int len, uint32_t dts, uint32_t pts) {
    assert(ctx && data && len > 0);
    DevChannel::Ptr *obj = (DevChannel::Ptr *) ctx;
    (*obj)->inputH265((char *) data, len, dts, pts);
}

API_EXPORT void API_CALL mk_media_input_aac(mk_media ctx, void *data, int len, uint32_t dts, int with_adts_header) {
    assert(ctx && data && len > 0);
    DevChannel::Ptr *obj = (DevChannel::Ptr *) ctx;
    (*obj)->inputAAC((char *) data, len, dts, with_adts_header);
}

API_EXPORT void API_CALL mk_media_input_aac1(mk_media ctx, void *data, int len, uint32_t dts, void *adts) {
    assert(ctx && data && len > 0 && adts);
    DevChannel::Ptr *obj = (DevChannel::Ptr *) ctx;
    (*obj)->inputAAC((char *) data, len, dts, (char *) adts);
}




