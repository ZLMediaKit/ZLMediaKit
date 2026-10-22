/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 * This file is part of ZLMediaKit (https://github.com/ZLMediaKit/ZLMediaKit).
 * Use of this source code is governed by the MIT-like license in the root directory.
 */
#include <cstdlib>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#include "../webrtc/WebRtcTransport.h"

using namespace mediakit;
using namespace toolkit;
using namespace std;
namespace {
void expect(bool value, const char *message) { if (!value) throw runtime_error(message); }
class TestTransport : public WebRtcTransportImp {
public:
    explicit TestTransport(const EventPoller::Ptr &poller) : WebRtcTransportImp(poller) {}
    vector<uint16_t> received;
    function<void(uint16_t)> observed;
protected:
    void onRecvRtp(MediaTrack &, const string &, RtpPacket::Ptr packet) override {
        received.push_back(packet->getSeq());
        if (observed) observed(packet->getSeq());
    }
};

struct Fixture {
    RtcMedia media;
    RtcCodecPlan primary;
    MediaTrack::Ptr track;
    TwccContext twcc;
    unique_ptr<WrappedRtpTrack> wrapper;
    Fixture(const shared_ptr<TestTransport> &transport, TrackType type) {
        media.type=type; primary.pt=96; primary.codec=type==TrackVideo ? "H264" : "opus";
        primary.sample_rate=type==TrackVideo ? 90000 : 48000;
        track=make_shared<MediaTrack>();track->media=&media;track->plan_rtp=&primary;
        track->offer_ssrc_rtp=1001;track->rtp_ext_ctx=make_shared<RtpExtContext>(media);
        wrapper.reset(new WrappedRtpTrack(track,twcc,*transport));
    }
    void input(uint16_t sequence,uint32_t ssrc=1001) {
        vector<uint8_t> bytes={0x80,96,static_cast<uint8_t>(sequence>>8),static_cast<uint8_t>(sequence),
            0,1,0x5f,0x90,static_cast<uint8_t>(ssrc>>24),static_cast<uint8_t>(ssrc>>16),
            static_cast<uint8_t>(ssrc>>8),static_cast<uint8_t>(ssrc),0x65,0x55};
        wrapper->inputRtp(reinterpret_cast<char *>(bytes.data()),bytes.size(),1000,
            reinterpret_cast<RtpHeader *>(bytes.data()));
    }
};

void deadline(const EventPoller::Ptr &poller,int limit,TrackType type) {
    auto done=make_shared<promise<void>>();auto ready=done->get_future();
    shared_ptr<TestTransport> transport;unique_ptr<Fixture> fixture;
    poller->sync([&]() {
        mINI::Instance()[Rtc::kRtpMaxWaitMS]=limit;
        mINI::Instance()[Rtc::kTwccIntervalMS]=0;
        transport=make_shared<TestTransport>(poller);
        transport->observed=[done](uint16_t seq) { if(seq==12) done->set_value(); };
        fixture.reset(new Fixture(transport,type));fixture->input(10);fixture->input(12);
    });
    const auto state=ready.wait_for(limit>0?chrono::milliseconds(2000):chrono::milliseconds(100));
    vector<uint16_t> received;
    poller->sync([&]() {
        received=transport->received;
        fixture.reset();transport.reset();
    });
    if(limit>0) {
        expect(state==future_status::ready,"actual RtpChannel timer failed on an idle queue");
        expect(received==vector<uint16_t>({10,12}),"deadline changed available packet order");
    } else {
        expect(state==future_status::timeout && received==vector<uint16_t>({10}),"disabled deadline changed legacy waiting");
    }
}
void feedbackOptionDoesNotEnableDeadline(const EventPoller::Ptr &poller) {
    shared_ptr<TestTransport> transport;unique_ptr<Fixture> fixture;
    poller->sync([&]() {
        mINI::Instance()[Rtc::kRtpMaxWaitMS]=0;
        mINI::Instance()[Rtc::kTwccIntervalMS]=20;
        transport=make_shared<TestTransport>(poller);fixture.reset(new Fixture(transport,TrackVideo));
        fixture->input(10);fixture->input(12);
    });
    this_thread::sleep_for(chrono::milliseconds(100));
    bool held=false;
    poller->sync([&]() {held=transport->received==vector<uint16_t>({10});fixture.reset();transport.reset();});
    expect(held,"TWCC configuration enabled the independent RTP deadline");
}
void destructionCancelsTimer(const EventPoller::Ptr &poller) {
    auto done=make_shared<promise<void>>();auto ready=done->get_future();
    weak_ptr<TestTransport> weak;
    poller->sync([&]() {
        mINI::Instance()[Rtc::kRtpMaxWaitMS]=80;
        auto transport=make_shared<TestTransport>(poller);weak=transport;
        transport->observed=[done](uint16_t seq) {if(seq==12)done->set_value();};
        Fixture fixture(transport,TrackVideo);fixture.input(10);fixture.input(12);
    });
    expect(weak.expired(),"deadline retained the transport");
    expect(ready.wait_for(chrono::milliseconds(150))==future_status::timeout,"destroyed channel callback still ran");
}
void sourceChange(const EventPoller::Ptr &poller) {
    shared_ptr<TestTransport> transport;unique_ptr<Fixture> fixture;
    poller->sync([&]() {
        mINI::Instance()[Rtc::kRtpMaxWaitMS]=50;
        transport=make_shared<TestTransport>(poller);fixture.reset(new Fixture(transport,TrackVideo));
        fixture->input(10000);
    });
    // RtpTrack accepts a different SSRC only after its existing 3-second guard.
    this_thread::sleep_for(chrono::milliseconds(3100));
    vector<uint16_t> received;
    poller->sync([&]() {
        fixture->input(1,2001);fixture->input(2,2001);received=transport->received;
        fixture.reset();transport.reset();
    });
    expect(received==vector<uint16_t>({10000,1,2}),"SSRC change retained an expired sequence window");
}
void retainedChannelAfterTransportDestruction(const EventPoller::Ptr &poller) {
    auto done=make_shared<promise<void>>();auto ready=done->get_future();
    unique_ptr<Fixture> fixture;weak_ptr<TestTransport> weak;
    poller->sync([&]() {
        mINI::Instance()[Rtc::kRtpMaxWaitMS]=20;
        auto transport=make_shared<TestTransport>(poller);weak=transport;
        transport->observed=[done](uint16_t seq) {if(seq==12)done->set_value();};
        fixture.reset(new Fixture(transport,TrackVideo));fixture->input(10);fixture->input(12);
    });
    const bool expired=weak.expired();
    const auto state=ready.wait_for(chrono::milliseconds(100));
    poller->sync([&]() {fixture.reset();});
    expect(expired && state==future_status::timeout,"retained channel called a destroyed transport");
}
void exceptionRearmsTimer(const EventPoller::Ptr &poller) {
    auto first=make_shared<promise<void>>();auto first_ready=first->get_future();
    auto second=make_shared<promise<void>>();auto second_ready=second->get_future();
    shared_ptr<TestTransport> transport;unique_ptr<Fixture> fixture;
    poller->sync([&]() {
        mINI::Instance()[Rtc::kRtpMaxWaitMS]=20;
        transport=make_shared<TestTransport>(poller);fixture.reset(new Fixture(transport,TrackVideo));
        transport->observed=[first,second](uint16_t seq) {
            if(seq==12) { first->set_value();throw runtime_error("deliberate receiver failure"); }
            if(seq==14) second->set_value();
        };
        fixture->input(10);fixture->input(12);
    });
    auto a=first_ready.wait_for(chrono::seconds(2));
    poller->sync([&]() { fixture->input(14); });
    auto b=second_ready.wait_for(chrono::seconds(2));
    poller->sync([&]() { fixture.reset();transport.reset(); });
    expect(a==future_status::ready && b==future_status::ready,"throwing receiver permanently disabled the timer");
}
} // namespace
int main() {
    EventPollerPool::setPoolSize(1);auto poller=EventPollerPool::Instance().getPoller();
    int passed=0,failed=0;
    auto run=[&](const char *name,const function<void()> &fn) {
        try {fn();++passed;cout<<"PASS "<<name<<endl;}
        catch(const exception &ex) {++failed;cerr<<"FAIL "<<name<<": "<<ex.what()<<endl;}
    };
    run("video_idle_timer",[&](){deadline(poller,50,TrackVideo);});
    run("audio_idle_timer",[&](){deadline(poller,50,TrackAudio);});
    run("default_disabled",[&](){deadline(poller,0,TrackVideo);});
    run("negative_disabled",[&](){deadline(poller,-1,TrackVideo);});
    run("independent_feedback_option",[&](){feedbackOptionDoesNotEnableDeadline(poller);});
    run("destroy_cancels_timer",[&](){destructionCancelsTimer(poller);});
    run("retained_channel_after_transport_destruction",[&](){retainedChannelAfterTransportDestruction(poller);});
    run("ssrc_change_resets_window",[&](){sourceChange(poller);});
    run("callback_exception_rearms_timer",[&](){exceptionRearmsTimer(poller);});
    cout<<"RESULT passed="<<passed<<" failed="<<failed<<endl;
    return failed?EXIT_FAILURE:EXIT_SUCCESS;
}
