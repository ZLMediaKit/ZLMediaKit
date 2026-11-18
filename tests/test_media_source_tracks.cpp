/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in
 * the LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "Common/MediaSource.h"
#include "Common/MultiMediaSourceMuxer.h"

using namespace mediakit;

namespace {

struct ThrowingListener : MediaSourceEvent {
    std::shared_ptr<MultiMediaSourceMuxer> getMuxer(MediaSource &) const override {
        throw std::runtime_error("getMuxer failed");
    }
};

struct TestSource : MediaSource {
    explicit TestSource(const std::string &stream)
        : MediaSource("rtsp", MediaTuple { "__defaultVhost__", "app", stream, "" }) {}
    int readerCount() override { return 0; }
};

bool testNoListener() {
    auto source = std::make_shared<TestSource>("no-listener");
    auto tracks = source->getTracks(false);
    if (!tracks.empty()) {
        std::cerr << "[FAIL] getTracks without listener should return empty tracks" << std::endl;
        return false;
    }
    std::cout << "[ OK ] getTracks without listener returns empty tracks" << std::endl;
    return true;
}

bool testDeadInterceptor() {
    auto source = std::make_shared<TestSource>("dead-interceptor");
    auto interceptor = std::make_shared<MediaSourceEventInterceptor>();
    {
        auto delegate = std::make_shared<MediaSourceEvent>();
        interceptor->setDelegate(delegate);
    }
    source->setListener(interceptor);

    auto tracks = source->getTracks(false);
    if (!tracks.empty()) {
        std::cerr << "[FAIL] getTracks should return empty when the interceptor delegate has expired" << std::endl;
        return false;
    }
    std::cout << "[ OK ] getTracks returns empty when the interceptor delegate has expired" << std::endl;
    return true;
}

bool testThrowingListener() {
    auto source = std::make_shared<TestSource>("throwing-listener");
    auto listener = std::make_shared<ThrowingListener>();
    source->setListener(listener);

    auto tracks = source->getTracks(false);
    if (!tracks.empty()) {
        std::cerr << "[FAIL] getTracks should return empty when getMuxer throws" << std::endl;
        return false;
    }
    std::cout << "[ OK ] getTracks returns empty when getMuxer throws" << std::endl;
    return true;
}

} // namespace

int main() {
    try {
        if (!testNoListener()) {
            return 1;
        }
        if (!testDeadInterceptor()) {
            return 2;
        }
        if (!testThrowingListener()) {
            return 3;
        }
    } catch (const std::exception &ex) {
        std::cerr << "[FAIL] unexpected exception: " << ex.what() << std::endl;
        return 4;
    }
    return 0;
}
