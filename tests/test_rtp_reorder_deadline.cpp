/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 * This file is part of ZLMediaKit (https://github.com/ZLMediaKit/ZLMediaKit).
 * Use of this source code is governed by the MIT-like license in the root directory.
 */
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "Rtsp/RtpReceiver.h"

using namespace mediakit;
using namespace std;

namespace {
void expect(bool value, const char *message) {
    if (!value) throw runtime_error(message);
}

class Sorter : public PacketSortor<int> {
public:
    uint64_t now = 1000;
    vector<uint16_t> output;
    explicit Sorter(size_t wait = 50) {
        setParams(1024, 3000, 512);
        setMaxWaitMS(wait);
        setOnSort([this](uint16_t seq, int) { output.push_back(seq); });
    }
    void input(uint16_t seq) { sortPacket(seq, seq); }
protected:
    uint64_t getSortTimeMS() const override { return now; }
};

void defaultBehavior() {
    Sorter s(0); s.input(10); s.input(12);
    s.now += 10000; s.flushExpiredPackets();
    expect(s.output == vector<uint16_t>({10}), "default mode must not add a deadline");
    expect(s.getPendingWaitMS() == 0, "default mode scheduled a timer");
    s.input(11);
    expect(s.output == vector<uint16_t>({10, 11, 12}), "default repair ordering changed");
}
void idleDeadline() {
    Sorter s; s.input(10); s.input(12); s.input(13);
    s.now += 49; s.flushExpiredPackets();
    expect(s.output.size() == 1 && s.getPendingWaitMS() == 1, "deadline fired early");
    ++s.now; s.flushExpiredPackets();
    expect(s.output == vector<uint16_t>({10, 12, 13}), "idle queue did not advance");
    expect(!s.getJitterSize() && !s.getPendingWaitMS(), "drained queue retained a deadline");
}
void duplicateKeepsDeadline() {
    Sorter s; s.input(10); s.input(12); s.now += 40; s.input(12);
    expect(s.getPendingWaitMS() == 10, "duplicate extended original arrival time");
    s.now += 10; s.flushExpiredPackets();
    expect(s.output == vector<uint16_t>({10, 12}), "duplicate was delivered twice");
}
void trickleDoesNotPostpone() {
    Sorter s; s.input(10); s.input(14); s.now += 40; s.input(11);
    s.now += 10; s.flushExpiredPackets();
    expect(s.output == vector<uint16_t>({10, 11, 14}), "partial progress postponed older buffered data");
}
void multipleGaps() {
    Sorter s; s.input(10); s.input(15); s.now += 10; s.input(13);
    s.now += 40; s.flushExpiredPackets();
    expect(s.output == vector<uint16_t>({10, 13, 15}), "expired packet remained behind another gap");
}
void repairBeforeDeadline() {
    Sorter s; s.input(10); s.input(12); s.now += 49; s.input(11);
    expect(s.output == vector<uint16_t>({10, 11, 12}), "timely repair was skipped");
    expect(s.getPendingWaitMS() == 0, "repaired queue kept a stale deadline");
}
void lateRepairAtDeadline() {
    Sorter s; s.input(10); s.input(12); s.now += 50; s.input(11);
    for (int i = 0; i < 1100; ++i) s.input(11);
    s.input(13);
    expect(s.output == vector<uint16_t>({10, 12, 13}), "late repair restarted expired media");
}
void sequenceWrap() {
    Sorter s; s.input(65534); s.input(0); s.input(1); s.now += 50;
    s.flushExpiredPackets(); s.input(65535); s.input(2);
    expect(s.output == vector<uint16_t>({65534, 0, 1, 2}), "sequence wrap or late RTX ordering failed");
}
void clearStartsNewSource() {
    Sorter s; s.input(10000); s.input(10002); s.clear();
    s.now += 1000; s.flushExpiredPackets(); s.input(1); s.input(2);
    expect(s.output == vector<uint16_t>({10000, 1, 2}), "clear leaked old source state");
    expect(s.getPendingWaitMS() == 0, "clear retained deadline state");
}
void disableAndEnable() {
    Sorter s; s.input(10); s.input(12); s.setMaxWaitMS(0);
    s.now += 100; s.flushExpiredPackets();
    expect(s.output.size() == 1, "zero did not disable deadline");
    s.setMaxWaitMS(30); s.now += 29; s.flushExpiredPackets();
    expect(s.output.size() == 1, "enabling fabricated an older arrival time");
    ++s.now; s.flushExpiredPackets();
    expect(s.output == vector<uint16_t>({10, 12}), "new deadline did not apply to pending data");
}
void callbackException() {
    Sorter s; s.input(10); s.input(12);
    s.setOnSort([](uint16_t, int) { throw runtime_error("application callback"); });
    s.now += 50;
    bool caught = false;
    try { s.flushExpiredPackets(); } catch (const runtime_error &) { caught = true; }
    expect(caught && !s.getJitterSize() && !s.getPendingWaitMS(), "exception left stale packet/deadline entries");
    s.flushExpiredPackets();
}
void defaultRollbackPreserved() {
    Sorter s(0); s.setParams(1024, 3000, 4); s.input(1000);
    // Stay outside the legacy sorter's near-zero wrap detection window.
    for (uint16_t seq = 100; seq <= 104; ++seq) s.input(seq);
    expect(s.output == vector<uint16_t>({1000, 100}), "legacy rollback detection changed");
}
void queueSizeFlushCleansDeadlines() {
    Sorter s; s.setParams(1, 3000, 512); s.input(10); s.input(12); s.input(13);
    expect(s.output == vector<uint16_t>({10, 12, 13}), "legacy packet-count trigger changed");
    expect(!s.getPendingWaitMS(), "packet-count flush left deadline metadata");
}
} // namespace

int main() {
    int passed = 0, failed = 0;
    auto run = [&](const char *name, const function<void()> &fn) {
        try { fn(); ++passed; cout << "PASS " << name << endl; }
        catch (const exception &ex) { ++failed; cerr << "FAIL " << name << ": " << ex.what() << endl; }
    };
    run("default_behavior", defaultBehavior);
    run("idle_deadline", idleDeadline);
    run("duplicate_keeps_deadline", duplicateKeepsDeadline);
    run("trickle_does_not_postpone", trickleDoesNotPostpone);
    run("multiple_gaps", multipleGaps);
    run("repair_before_deadline", repairBeforeDeadline);
    run("late_repair_at_deadline", lateRepairAtDeadline);
    run("sequence_wrap", sequenceWrap);
    run("clear_starts_new_source", clearStartsNewSource);
    run("disable_and_enable", disableAndEnable);
    run("callback_exception", callbackException);
    run("default_rollback_preserved", defaultRollbackPreserved);
    run("queue_size_flush_cleans_deadlines", queueSizeFlushCleansDeadlines);
    cout << "RESULT passed=" << passed << " failed=" << failed << endl;
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
