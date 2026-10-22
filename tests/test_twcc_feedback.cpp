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
    explicit Capture(bool periodic = false) {
        context.setPeriodicFeedback(periodic);
        context.setOnSendTwccCB([this](uint32_t ssrc, string fci) {
            reports.push_back(reinterpret_cast<const FCI_TWCC *>(fci.data())->getPacketChunkList(fci.size()));
            sources.push_back(ssrc);
        });
    }
};
void emptyAndSingle() {
    Capture c(true); c.context.flushPending(); expect(c.reports.empty(), "empty feedback emitted");
    c.context.onRtp(1234, 10, 1000);
    expect(c.reports.empty(), "single arrival changed default trigger");
    c.context.flushPending(); c.context.flushPending();
    expect(c.reports.size() == 1 && c.reports[0].count(10) && c.sources[0] == 1234, "pending/empty flush incorrect");
}
void duplicateSource() {
    Capture c(true); c.context.onRtp(1234, 10, 1000); c.context.onRtp(9999, 10, 1001); c.context.flushPending();
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
    Capture c(true); c.context.onRtp(1234, 65535, 1000); c.context.flushPending();
    c.context.onRtp(5678, 0, 1001); c.context.flushPending();
    expect(c.reports.size() == 2 && c.reports[0].count(65535) && c.reports[1].count(0), "flush across wrap lost statuses");
    expect(c.sources[1] == 5678, "new batch retained stale source");
}
void reorderedArrival() {
    Capture c(true); c.context.onRtp(1234, 11, 1000); c.context.onRtp(1234, 10, 1001); c.context.flushPending();
    expect(c.reports[0].at(11).second == -4, "reordered transport sequence lost negative receive delta");
}
bool received(const FCI_TWCC::TwccPacketStatus &report, uint16_t seq) {
    auto it=report.find(seq);
    return it!=report.end() && it->second.first!=SymbolStatus::not_received;
}
bool missing(const FCI_TWCC::TwccPacketStatus &report, uint16_t seq) {
    auto it=report.find(seq);
    return it!=report.end() && it->second.first==SymbolStatus::not_received;
}
void crossBatchLoss() {
    Capture c(true);
    c.context.onRtp(1234,10,1000);c.context.flushPending();
    c.context.onRtp(1234,12,1001);c.context.flushPending();
    expect(c.reports.size()==2 && c.reports[1].size()==2 && missing(c.reports[1],11) && received(c.reports[1],12),
           "cross-batch loss 11 was omitted");
}
void adjacentBatches() {
    Capture c(true);
    c.context.onRtp(1234,10,1000);c.context.flushPending();
    c.context.onRtp(1234,11,1001);c.context.flushPending();
    expect(c.reports.size()==2 && c.reports[1].size()==1 && received(c.reports[1],11),"adjacent batch repeated an acknowledged packet");
}
void emptyTicksPreserveGap() {
    Capture c(true);
    c.context.onRtp(1234,10,1000);c.context.flushPending();
    c.context.flushPending();c.context.flushPending();
    expect(c.reports.size()==1,"empty timer ticks emitted feedback");
    c.context.onRtp(1234,14,1004);c.context.flushPending();
    expect(c.reports.size()==2 && c.reports[1].size()==4 && missing(c.reports[1],11) && missing(c.reports[1],12)
           && missing(c.reports[1],13) && received(c.reports[1],14),"empty ticks advanced the unreported sequence");
}
void reorderedNewBatch() {
    Capture c(true);
    c.context.onRtp(1234,10,1000);c.context.flushPending();
    c.context.onRtp(1234,13,1001);c.context.onRtp(1234,11,1002);c.context.onRtp(1234,12,1003);c.context.flushPending();
    expect(c.reports.size()==2 && c.reports[1].size()==3 && received(c.reports[1],11) && received(c.reports[1],12)
           && received(c.reports[1],13),"reordered pending arrivals were reported missing");
    expect(c.reports[1].at(12).second==4 && c.reports[1].at(13).second==-8,"reordering corrupted wire receive deltas");
}
void gapPartlyRepairedBeforeFlush() {
    Capture c(true);
    c.context.onRtp(1234,10,1000);c.context.flushPending();
    c.context.onRtp(1234,13,1001);c.context.onRtp(1234,11,1002);c.context.flushPending();
    expect(c.reports.size()==2 && received(c.reports[1],11) && missing(c.reports[1],12) && received(c.reports[1],13),
           "partially repaired batch lost a received or missing status");
}
void lateCorrectionPreservesProgress() {
    Capture c(true);
    c.context.onRtp(1234,10,1000);c.context.flushPending();
    c.context.onRtp(1234,12,1001);c.context.flushPending();
    c.context.onRtp(1234,11,1002);c.context.flushPending();
    expect(c.reports.size()==3 && c.reports[2].size()==1 && received(c.reports[2],11),"late arrival was not corrected");
    c.context.onRtp(1234,14,1003);c.context.flushPending();
    expect(c.reports.size()==4 && c.reports[3].size()==2 && missing(c.reports[3],13) && received(c.reports[3],14)
           && !c.reports[3].count(12),"late correction moved the forward cursor backwards");
}
void duplicateOldArrivalPreservesProgress() {
    Capture c(true);
    c.context.onRtp(1234,10,1000);c.context.flushPending();
    c.context.onRtp(1234,12,1001);c.context.flushPending();
    c.context.onRtp(1234,10,1002);c.context.flushPending();
    c.context.onRtp(1234,14,1003);c.context.flushPending();
    const auto &last=c.reports.back();
    expect(last.size()==2 && missing(last,13) && received(last,14),"duplicate old arrival reopened an acknowledged range");
}
void sparseLateCorrections() {
    Capture c(true);
    c.context.onRtp(1234,10,1000);c.context.flushPending();
    c.context.onRtp(1234,14,1001);c.context.flushPending();
    c.context.onRtp(1234,12,1002);c.context.flushPending();
    const auto count=c.reports.size();
    c.context.onRtp(1234,11,1003);c.context.onRtp(1234,13,1004);c.context.flushPending();
    expect(c.reports.size()==count+2 && c.reports[count].size()==1 && received(c.reports[count],11)
           && c.reports[count+1].size()==1 && received(c.reports[count+1],13),"sparse late correction falsely reopened an acknowledged gap");
}
void mixedLateAndForwardBatch() {
    Capture c(true);
    c.context.onRtp(1234,10,1000);c.context.flushPending();
    c.context.onRtp(1234,12,1001);c.context.flushPending();
    c.context.onRtp(1234,11,1002);c.context.onRtp(1234,14,1003);c.context.flushPending();
    expect(c.reports.size()==4 && c.reports[2].size()==1 && received(c.reports[2],11)
           && c.reports[3].size()==2 && missing(c.reports[3],13) && received(c.reports[3],14),
           "mixed late/forward batch moved progress or repeated previous acknowledgements");
}
void wrapGapBetweenBatches() {
    Capture c(true);
    c.context.onRtp(1234,65534,1000);c.context.flushPending();
    c.context.onRtp(1234,1,1001);c.context.flushPending();
    expect(c.reports.size()==3 && c.reports[1].size()==1 && missing(c.reports[1],65535)
           && c.reports[2].size()==2 && missing(c.reports[2],0) && received(c.reports[2],1),"cross-wrap loss was omitted or misencoded");
}
void wrapGapInPendingBatch() {
    Capture c(true);
    c.context.onRtp(1234,65534,1000);c.context.onRtp(1234,0,1001);c.context.flushPending();
    expect(c.reports.size()==2 && c.reports[0].size()==2 && received(c.reports[0],65534) && missing(c.reports[0],65535)
           && c.reports[1].size()==1 && received(c.reports[1],0),"pending wrap lost the trailing missing status");
}
void latePreWrapCorrection() {
    Capture c(true);
    c.context.onRtp(1234,65534,1000);c.context.flushPending();
    c.context.onRtp(1234,1,1001);c.context.flushPending();
    c.context.onRtp(1234,65535,1002);c.context.flushPending();
    expect(received(c.reports.back(),65535),"late pre-wrap arrival was discarded");
    c.context.onRtp(1234,3,1003);c.context.flushPending();
    expect(c.reports.back().size()==2 && missing(c.reports.back(),2) && received(c.reports.back(),3),"late pre-wrap packet reset progress");
}
void reorderedAcrossWrap() {
    Capture c(true);
    c.context.onRtp(1234,65535,1000);c.context.onRtp(1234,1,1001);c.context.onRtp(1234,0,1002);c.context.flushPending();
    expect(c.reports.size()==2 && c.reports[0].size()==1 && received(c.reports[0],65535) && c.reports[1].size()==2
           && received(c.reports[1],0) && received(c.reports[1],1),"reordered wrap lost an arrival");
    expect(c.reports[1].at(1).second==-4,"wrapped report corrupted the negative receive delta");
}
void wrapWithoutGap() {
    Capture c(true);
    c.context.onRtp(1234,65535,1000);c.context.flushPending();c.context.flushPending();
    c.context.onRtp(1234,0,1001);c.context.flushPending();
    expect(c.reports.size()==2 && c.reports[1].size()==1 && received(c.reports[1],0),"gap-free wrap repeated a report");
}
void periodicCountTriggerContinuity() {
    Capture c(true);
    c.context.onRtp(1234,10,1000);c.context.flushPending();
    for (uint16_t i=12;i<32;++i)c.context.onRtp(1234,i,1000+i);
    expect(c.reports.size()==2 && c.reports[1].size()==21 && missing(c.reports[1],11),"count-triggered report omitted cross-batch gap");
    c.context.onRtp(1234,33,1040);c.context.flushPending();
    expect(missing(c.reports.back(),32) && received(c.reports.back(),33),"count trigger failed to advance the cursor");
}
void periodicTimeTriggerContinuity() {
    Capture c(true);
    c.context.onRtp(1234,10,1000);c.context.flushPending();
    c.context.onRtp(1234,12,1100);c.context.onRtp(1234,13,1356);
    expect(c.reports.size()==2 && missing(c.reports[1],11) && received(c.reports[1],12) && received(c.reports[1],13),
           "time-triggered report omitted cross-batch gap");
}
void disabledKeepsLegacyBatches() {
    Capture c;
    c.context.onRtp(1234,10,1000);c.context.onRtp(1234,11,1256);
    c.context.onRtp(1234,13,1300);c.context.onRtp(1234,14,1556);
    expect(c.reports.size()==2 && c.reports[1].size()==2 && !c.reports[1].count(12)
           && received(c.reports[1],13) && received(c.reports[1],14),"disabled periodic mode changed legacy feedback");
}
void sourceChangePreservesTransportProgress() {
    Capture c(true);
    c.context.onRtp(1234,10,1000);c.context.flushPending();
    c.context.onRtp(5678,12,1001);c.context.flushPending();
    expect(c.reports.size()==2 && missing(c.reports[1],11) && received(c.reports[1],12) && c.sources[1]==5678,
           "SSRC change reset transport-wide feedback progress");
}
void referenceAcrossTwoWraps() {
    TwccContext context;
    context.setPeriodicFeedback(true);
    uint32_t next=65520;
    context.setOnSendTwccCB([&](uint32_t,string fci) {
        const auto *wire=reinterpret_cast<const FCI_TWCC *>(fci.data());
        expect(wire->getBaseSeq()==static_cast<uint16_t>(next),"reference: feedback skipped or repeated a sequence");
        const auto statuses=wire->getPacketChunkList(fci.size());
        for (uint16_t i=0;i<wire->getPacketCount();++i,++next) {
            const auto seq=static_cast<uint16_t>(next);
            expect(received(statuses,seq)==((next-65520)%11!=3),"reference: incorrect received/missing status");
        }
    });
    for (uint32_t i=0;i<70000;++i) {
        if(i%11!=3)context.onRtp(1234,static_cast<uint16_t>(65520+i),1000+i);
        if(i%7==6)context.flushPending();
    }
    context.flushPending();
    expect(next==135520,"reference: final feedback range is incomplete");
}
void idleTimer() {
    EventPollerPool::setPoolSize(1);
    auto poller=EventPollerPool::Instance().getPoller();
    auto context=make_shared<TwccContext>();
    auto completed=make_shared<promise<void>>(); auto future=completed->get_future();
    Timer::Ptr timer;
    poller->sync([&]() {
        context->setPeriodicFeedback(true);
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
void actualTimerCrossBatchGap() {
    auto poller=EventPollerPool::Instance().getPoller();
    auto context=make_shared<TwccContext>();
    auto first=make_shared<promise<void>>();auto first_ready=first->get_future();
    auto second=make_shared<promise<bool>>();auto second_ready=second->get_future();
    Timer::Ptr timer;
    poller->sync([&]() {
        context->setPeriodicFeedback(true);
        context->setOnSendTwccCB([first,second](uint32_t,string fci) {
            auto report=reinterpret_cast<const FCI_TWCC *>(fci.data())->getPacketChunkList(fci.size());
            if(received(report,10))first->set_value();
            if(received(report,12))second->set_value(missing(report,11));
        });
        context->onRtp(1234,10,1000);
        timer=make_shared<Timer>(.02f,[context]() { context->flushPending();return true; },poller);
    });
    const bool first_sent=first_ready.wait_for(chrono::seconds(2))==future_status::ready;
    if(first_sent)poller->sync([&]() { context->onRtp(1234,12,1001); });
    const bool second_sent=first_sent && second_ready.wait_for(chrono::seconds(2))==future_status::ready;
    poller->sync([&]() { timer=nullptr;context.reset(); });
    expect(second_sent && second_ready.get(),"real periodic timer omitted a cross-batch loss");
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
    run("cross_batch_loss",crossBatchLoss);run("adjacent_batches",adjacentBatches);
    run("empty_ticks_preserve_gap",emptyTicksPreserveGap);run("reordered_new_batch",reorderedNewBatch);
    run("partly_repaired_gap",gapPartlyRepairedBeforeFlush);run("late_correction_progress",lateCorrectionPreservesProgress);
    run("old_duplicate_progress",duplicateOldArrivalPreservesProgress);run("wrap_gap_between_batches",wrapGapBetweenBatches);
    run("sparse_late_corrections",sparseLateCorrections);run("mixed_late_forward",mixedLateAndForwardBatch);
    run("wrap_gap_pending",wrapGapInPendingBatch);run("late_pre_wrap_correction",latePreWrapCorrection);
    run("reordered_across_wrap",reorderedAcrossWrap);run("wrap_without_gap",wrapWithoutGap);
    run("periodic_count_continuity",periodicCountTriggerContinuity);run("periodic_time_continuity",periodicTimeTriggerContinuity);
    run("disabled_legacy_batches",disabledKeepsLegacyBatches);run("source_change_continuity",sourceChangePreservesTransportProgress);
    run("reference_two_wraps",referenceAcrossTwoWraps);
    run("actual_timer_cross_batch_gap",actualTimerCrossBatchGap);
    cout<<"RESULT passed="<<passed<<" failed="<<failed<<endl;
    return failed?EXIT_FAILURE:EXIT_SUCCESS;
}
