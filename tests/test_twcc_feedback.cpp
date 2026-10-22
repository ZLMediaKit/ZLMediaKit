/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 * This file is part of ZLMediaKit (https://github.com/ZLMediaKit/ZLMediaKit).
 * Use of this source code is governed by the MIT-like license in the root directory.
 */
#include <cstdlib>
#include <iostream>
#include <future>
#include <stdexcept>
#include <vector>
#include "../webrtc/TwccContext.h"
#include "Rtcp/RtcpFCI.h"
#include "Poller/EventPoller.h"
#include "Poller/Timer.h"

using namespace mediakit;
using namespace toolkit;
using namespace std;
namespace {
void expect(bool value, const char *message) { if (!value) throw runtime_error(message); }
struct Capture {
    TwccContext context;
    vector<FCI_TWCC::TwccPacketStatus> reports;
    vector<uint32_t> sources;
    Capture() {
        context.setOnSendTwccCB([this](uint32_t ssrc, string fci) {
            reports.push_back(reinterpret_cast<const FCI_TWCC *>(fci.data())->getPacketChunkList(fci.size()));
            sources.push_back(ssrc);
        });
    }
};
void emptyAndSingle() {
    Capture c; c.context.flushPending(); expect(c.reports.empty(), "empty feedback emitted");
    c.context.onRtp(1234, 10, 1000);
    expect(c.reports.empty(), "single arrival changed default trigger");
    c.context.flushPending(); c.context.flushPending();
    expect(c.reports.size() == 1 && c.reports[0].count(10) && c.sources[0] == 1234, "pending/empty flush incorrect");
}
void duplicateSource() {
    Capture c; c.context.onRtp(1234, 10, 1000); c.context.onRtp(9999, 10, 1001); c.context.flushPending();
    expect(c.sources[0] == 1234, "duplicate changed pending batch source");
}
void countTrigger() {
    Capture c;
    for (uint16_t i=0; i<20; ++i) c.context.onRtp(1234, i, 1000+i);
    expect(c.reports.size() == 1 && c.reports[0].size() == 20, "existing count trigger changed");
    c.context.flushPending(); expect(c.reports.size() == 1, "empty periodic duplicate report");
}
void timeTrigger() {
    Capture c; c.context.onRtp(1234, 10, 1000); c.context.onRtp(1234, 11, 1256);
    expect(c.reports.size() == 1, "existing arrival-span trigger changed");
}
void sequenceWrap() {
    Capture c; c.context.onRtp(1234, 65535, 1000); c.context.flushPending();
    c.context.onRtp(5678, 0, 1001); c.context.flushPending();
    expect(c.reports.size() == 2 && c.reports[0].count(65535) && c.reports[1].count(0), "flush across wrap lost statuses");
    expect(c.sources[1] == 5678, "new batch retained stale source");
}
void reorderedArrival() {
    Capture c; c.context.onRtp(1234, 11, 1000); c.context.onRtp(1234, 10, 1001); c.context.flushPending();
    expect(c.reports[0].at(11).second == -4, "reordered transport sequence lost negative receive delta");
}
void idleTimer() {
    EventPollerPool::setPoolSize(1);
    auto poller=EventPollerPool::Instance().getPoller();
    auto context=make_shared<TwccContext>();
    auto completed=make_shared<promise<void>>(); auto future=completed->get_future();
    Timer::Ptr timer;
    poller->sync([&]() {
        context->setOnSendTwccCB([completed](uint32_t, string) { completed->set_value(); });
        context->onRtp(1234, 10, 1000);
        weak_ptr<TwccContext> weak=context;
        timer=make_shared<Timer>(.02f,[weak]() {
            auto locked=weak.lock(); if (!locked) return false;
            locked->flushPending(); return true;
        },poller);
    });
    bool ready=future.wait_for(chrono::seconds(2))==future_status::ready;
    poller->sync([&]() { timer=nullptr;context.reset(); });
    expect(ready,"idle feedback needed an additional RTP packet");
}
} // namespace
int main() {
    int passed=0,failed=0;
    auto run=[&](const char *name,const function<void()> &fn) {
        try { fn();++passed;cout<<"PASS "<<name<<endl; }
        catch(const exception &ex) { ++failed;cerr<<"FAIL "<<name<<": "<<ex.what()<<endl; }
    };
    run("empty_and_single",emptyAndSingle); run("duplicate_source",duplicateSource);
    run("count_trigger",countTrigger);run("time_trigger",timeTrigger);
    run("sequence_wrap",sequenceWrap);run("reordered_arrival",reorderedArrival);run("idle_timer",idleTimer);
    cout<<"RESULT passed="<<passed<<" failed="<<failed<<endl;
    return failed?EXIT_FAILURE:EXIT_SUCCESS;
}
