/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "HlsRecorder.h"

namespace mediakit {

ThreadPool &getHlsThread() {
    static ThreadPool ret(1, ThreadPool::PRIORITY_LOWEST, true);
    static onceToken s_token([]() {
        ret.async([]() {
            setThreadName("hls thread");
        });
    });
    return ret;
}

bool HlsRecorder::inputFrame(const Frame::Ptr &frame) {
    auto ptr = shared_from_this();
    auto cached_frame = Frame::getCacheAbleFrame(frame);
    getHlsThread().async([ptr, cached_frame]() {
        ptr->inputFrame_l(cached_frame);
    });
    return true;
}

}//namespace mediakit
