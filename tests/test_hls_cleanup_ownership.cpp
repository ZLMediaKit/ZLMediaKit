/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of this source tree.
 */

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#if defined(ENABLE_HLS)
#include "Common/config.h"
#include "Record/HlsMakerImp.h"
#include "Record/MPEG.h"
#include "Util/File.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

class MpegMuxerTestAccess {
public:
    static void setCurrentBuffer(MpegMuxer &muxer, size_t size) {
        auto buffer = muxer._buffer_pool.obtain2();
        buffer->setCapacity(size);
        memset(buffer->data(), 0x47, size);
        buffer->setSize(size);
        muxer._current_buffer = std::move(buffer);
    }
};

} // namespace mediakit

using namespace mediakit;

namespace {

class ScopedTestDirectory {
public:
    ScopedTestDirectory() {
        auto nonce = chrono::steady_clock::now().time_since_epoch().count();
        path = File::absolutePath("hls-cleanup-owner-" + to_string(nonce), ".");
    }

    ~ScopedTestDirectory() {
        File::delete_file(path);
    }

    string path;
};

class MpegMuxerProbe final : public MpegMuxer {
public:
    struct Event {
        bool reset;
        size_t size;
    };

    vector<Event> events;

private:
    void onWrite(Buffer::Ptr buffer, uint64_t, bool) override {
        events.emplace_back(Event{!buffer, buffer ? buffer->size() : 0});
    }
};

void expect(bool condition, const string &message) {
    if (!condition) {
        throw runtime_error(message);
    }
}

void configureHlsRecorder(uint32_t delete_delay_sec) {
    mINI::Instance()[Hls::kSegmentDuration] = 2;
    mINI::Instance()[Hls::kSegmentMaxDuration] = 0;
    mINI::Instance()[Hls::kSegmentNum] = 3;
    mINI::Instance()[Hls::kSegmentKeep] = false;
    mINI::Instance()[Hls::kFileBufSize] = 4096;
    mINI::Instance()[Hls::kDeleteDelaySec] = delete_delay_sec;
    mINI::Instance()[Hls::kFmp4SegExt] = ".m4s";
    mINI::Instance()[Hls::kSegmentDelay] = 0;
    NOTICE_EMIT(BroadcastReloadConfigArgs, Broadcast::kBroadcastReloadConfig);
}

string findPlaylistUri(const string &index, const string &suffix) {
    size_t line_begin = 0;
    while (line_begin < index.size()) {
        auto line_end = index.find('\n', line_begin);
        if (line_end == string::npos) {
            line_end = index.size();
        }
        auto query = index.find('?', line_begin);
        auto uri_end = query != string::npos && query < line_end ? query : line_end;
        auto uri_size = uri_end - line_begin;
        if (uri_size >= suffix.size()
            && index.compare(uri_end - suffix.size(), suffix.size(), suffix) == 0) {
            return index.substr(line_begin, uri_size);
        }
        line_begin = line_end + 1;
    }
    throw runtime_error("HLS index is missing the expected media URI");
}

string writeSession(HlsMakerImp &maker, const string &index_path, char marker) {
    const char init[] = {marker, marker, marker, marker};
    maker.inputInitSegment(init, sizeof(init));
    maker.inputData(&marker, 1, 0, true);
    maker.inputData(&marker, 1, 100, false);
    maker.inputData(nullptr, 0, 100, false);
    auto segment_uri = findPlaylistUri(File::loadFile(index_path), ".m4s");
    return File::parentDir(index_path) + segment_uri;
}

void expectSessionFiles(const string &index_path, const string &segment_path, bool exist) {
    auto init_path = File::parentDir(index_path) + "init.mp4";
    expect(File::fileExist(index_path) == exist, "unexpected replacement playlist state");
    expect(File::fileExist(init_path) == exist, "unexpected replacement init state");
    expect(File::fileExist(segment_path) == exist, "unexpected replacement segment state");
}

void waitForSessionCleanup(const string &index_path, const string &segment_path) {
    auto init_path = File::parentDir(index_path) + "init.mp4";
    for (size_t i = 0; i < 250; ++i) {
        if (!File::fileExist(index_path) && !File::fileExist(init_path) && !File::fileExist(segment_path)) {
            return;
        }
        this_thread::sleep_for(chrono::milliseconds(20));
    }
    throw runtime_error("replacement session files were not reclaimed before timeout");
}

void testMpegFlushDrainsCurrentBuffer() {
    MpegMuxerProbe muxer;
    MpegMuxerTestAccess::setCurrentBuffer(muxer, 188);
    muxer.flush();
    expect(muxer.events.size() == 1, "flush did not emit the pending MPEG buffer");
    expect(!muxer.events.front().reset && muxer.events.front().size == 188,
           "flush emitted an empty or reset event");

    muxer.events.clear();
    MpegMuxerTestAccess::setCurrentBuffer(muxer, 188);
    muxer.resetTracks();
    expect(muxer.events.size() == 2, "reset did not emit the MPEG tail and reset notification");
    expect(!muxer.events.front().reset && muxer.events.front().size == 188,
           "reset did not emit the MPEG tail first");
    expect(muxer.events.back().reset, "reset notification was not emitted last");
}

void testLateDelayedCleanupWaitsForReplacement() {
    configureHlsRecorder(1);
    ScopedTestDirectory directory;
    auto index_path = directory.path + "/live.m3u8";

    std::unique_ptr<HlsMakerImp> old_session(
        new HlsMakerImp(true, index_path, "owner=old", 4096, 2, 3, false, ".m4s"));
    auto old_segment_path = writeSession(*old_session, index_path, 'O');

    std::unique_ptr<HlsMakerImp> replacement(
        new HlsMakerImp(true, index_path, "owner=new", 4096, 2, 3, false, ".m4s"));
    replacement->clearCache();
    old_session.reset();
    auto replacement_segment_path = writeSession(*replacement, index_path, 'N');

    this_thread::sleep_for(chrono::milliseconds(1500));
    expectSessionFiles(index_path, replacement_segment_path, true);

    replacement.reset();
    waitForSessionCleanup(index_path, replacement_segment_path);
    expect(!File::fileExist(old_segment_path), "old media segment was not eventually reclaimed");
}

void testLateImmediateCleanupWaitsForReplacement() {
    configureHlsRecorder(0);
    ScopedTestDirectory directory;
    auto index_path = directory.path + "/live.m3u8";

    std::unique_ptr<HlsMakerImp> old_session(
        new HlsMakerImp(true, index_path, "owner=old", 4096, 2, 3, false, ".m4s"));
    auto old_segment_path = writeSession(*old_session, index_path, 'O');

    std::unique_ptr<HlsMakerImp> replacement(
        new HlsMakerImp(true, index_path, "owner=new", 4096, 2, 3, false, ".m4s"));
    replacement->clearCache();
    auto replacement_segment_path = writeSession(*replacement, index_path, 'N');
    old_session.reset();

    expectSessionFiles(index_path, replacement_segment_path, true);
    auto replacement_index = File::loadFile(index_path);
    expect(replacement_index.find("owner=new") != string::npos
               && replacement_index.find("owner=old") == string::npos,
           "stale EOF rewrote the replacement playlist");
    replacement.reset();
    expectSessionFiles(index_path, replacement_segment_path, false);
    expect(!File::fileExist(old_segment_path), "old media segment was not reclaimed by final cleanup");
}

void runTest(const string &name) {
    if (name == "mpeg-flush") {
        testMpegFlushDrainsCurrentBuffer();
    } else if (name == "cleanup-delayed-late" || name == "cleanup-final-drain") {
        testLateDelayedCleanupWaitsForReplacement();
    } else if (name == "cleanup-immediate-late") {
        testLateImmediateCleanupWaitsForReplacement();
    } else {
        throw runtime_error("unknown test name");
    }
}

} // namespace

int main(int argc, char **argv) {
    try {
        if (argc == 2) {
            runTest(argv[1]);
        } else {
            testMpegFlushDrainsCurrentBuffer();
            testLateDelayedCleanupWaitsForReplacement();
            testLateImmediateCleanupWaitsForReplacement();
        }
        configureHlsRecorder(0);
        cout << "test_hls_cleanup_ownership passed" << endl;
        return 0;
    } catch (const exception &ex) {
        configureHlsRecorder(0);
        cerr << "test_hls_cleanup_ownership failed: " << ex.what() << endl;
        return EXIT_FAILURE;
    }
}
#else
int main() {
    return 0;
}
#endif
