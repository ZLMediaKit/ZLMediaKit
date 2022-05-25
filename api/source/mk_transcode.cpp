/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "mk_transcode.h"
#include "Extension/Track.h"

using namespace mediakit;

#ifdef ENABLE_FFMPEG

#include "Codec/Transcode.h"

API_EXPORT mk_decoder API_CALL mk_decoder_create(mk_track track, int thread_num) {
    assert(track);
    return new FFmpegDecoder(*((Track::Ptr *)track), thread_num);
}

API_EXPORT void API_CALL mk_decoder_release(mk_decoder ctx, int flush_frame) {
    assert(ctx);
    auto decoder = (FFmpegDecoder *)ctx;
    if (flush_frame) {
        decoder->stopThread(false);
    }
    delete decoder;
}

API_EXPORT void API_CALL mk_decoder_decode(mk_decoder ctx, mk_frame frame, int async, int enable_merge) {
    assert(ctx && frame);
    ((FFmpegDecoder *)ctx)->inputFrame(*((Frame::Ptr *)frame), false, async, enable_merge);
}

API_EXPORT void API_CALL mk_decoder_set_max_async_frame_size(mk_decoder ctx, size_t size) {
    assert(ctx && size);
    ((FFmpegDecoder *)ctx)->setMaxTaskSize(size);
}

API_EXPORT void API_CALL mk_decoder_set_cb(mk_decoder ctx, on_mk_decode cb, void *user_data) {
    assert(ctx && cb);
    ((FFmpegDecoder *)ctx)->setOnDecode([cb, user_data](const FFmpegFrame::Ptr &pix_frame) {
        cb(user_data, (mk_frame_pix)&pix_frame);
    });
}

API_EXPORT const AVCodecContext *API_CALL mk_decoder_get_context(mk_decoder ctx) {
    assert(ctx);
    return ((FFmpegDecoder *)ctx)->getContext();
}

/////////////////////////////////////////////////////////////////////////////////////////////

API_EXPORT mk_frame_pix API_CALL mk_frame_pix_ref(mk_frame_pix frame) {
    assert(frame);
    return new FFmpegFrame::Ptr(*(FFmpegFrame::Ptr *)frame);
}

API_EXPORT mk_frame_pix API_CALL mk_frame_pix_from_av_frame(AVFrame *frame) {
    assert(frame);
    return new FFmpegFrame::Ptr(std::make_shared<FFmpegFrame>(
        std::shared_ptr<AVFrame>(av_frame_clone(frame), [](AVFrame *frame) { av_frame_free(&frame); })));
}

API_EXPORT void API_CALL mk_frame_pix_unref(mk_frame_pix frame) {
    assert(frame);
    delete (FFmpegFrame::Ptr *)frame;
}

API_EXPORT AVFrame *API_CALL mk_frame_pix_get_av_frame(mk_frame_pix frame) {
    assert(frame);
    return (*(FFmpegFrame::Ptr *)frame)->get();
}

//////////////////////////////////////////////////////////////////////////////////

API_EXPORT mk_swscale mk_swscale_create(int output, int width, int height) {
    return new FFmpegSws((AVPixelFormat)output, width, height);
}

API_EXPORT void mk_swscale_release(mk_swscale ctx) {
    delete (FFmpegSws *)ctx;
}

API_EXPORT int mk_swscale_input_frame(mk_swscale ctx, mk_frame_pix frame, uint8_t *data) {
    return ((FFmpegSws *)ctx)->inputFrame(*(FFmpegFrame::Ptr *)frame, data);
}

API_EXPORT mk_frame_pix mk_swscale_input_frame2(mk_swscale ctx, mk_frame_pix frame) {
    return new FFmpegFrame::Ptr(((FFmpegSws *)ctx)->inputFrame(*(FFmpegFrame::Ptr *)frame));
}

API_EXPORT uint8_t **API_CALL mk_get_av_frame_data(AVFrame *frame) {
    return frame->data;
}

API_EXPORT int *API_CALL mk_get_av_frame_line_size(AVFrame *frame) {
    return frame->linesize;
}

API_EXPORT int64_t API_CALL mk_get_av_frame_dts(AVFrame *frame) {
    return frame->pkt_dts;
}

API_EXPORT int64_t API_CALL mk_get_av_frame_pts(AVFrame *frame) {
    return frame->pts;
}

API_EXPORT int API_CALL mk_get_av_frame_width(AVFrame *frame) {
    return frame->width;
}

API_EXPORT int API_CALL mk_get_av_frame_height(AVFrame *frame) {
    return frame->height;
}

API_EXPORT int API_CALL mk_get_av_frame_format(AVFrame *frame) {
    return frame->format;
}


#endif //ENABLE_FFMPEG