/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 * Use of this source code is governed by MIT-like license in the root directory.
 */

#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "../webrtc/WebRtcTransport.h"
#include "Rtcp/RtcpFCI.h"

using namespace mediakit;
using namespace toolkit;
using namespace std;

namespace {

void expect(bool condition, const string &message) {
    if (!condition) {
        throw runtime_error(message);
    }
}

struct ReceivedPacket {
    uint16_t seq;
    uint32_t ssrc;
    string payload;
};

class TestTransport : public WebRtcTransportImp {
public:
    explicit TestTransport(const EventPoller::Ptr &poller) : WebRtcTransportImp(poller) {}
    vector<ReceivedPacket> received;

protected:
    void onRecvRtp(MediaTrack &, const string &, RtpPacket::Ptr packet) override {
        received.push_back({packet->getSeq(), packet->getSSRC(),
            string(reinterpret_cast<char *>(packet->getPayload()), packet->getPayloadSize())});
    }
};

void put16(vector<uint8_t> &bytes, uint16_t value) {
    bytes.push_back(value >> 8);
    bytes.push_back(value & 0xff);
}

void put32(vector<uint8_t> &bytes, uint32_t value) {
    put16(bytes, value >> 16);
    put16(bytes, value & 0xffff);
}

// Produce an actual RFC 8285 extension and RFC 4588 RTX payload. Transport
// sequence, RTX RTP sequence, and original RTP sequence deliberately differ.
vector<uint8_t> makePacket(bool rtx, uint32_t ssrc, uint16_t rtp_seq,
    uint16_t original_seq, uint16_t transport_seq, uint8_t ext_id,
    bool has_twcc, bool two_byte, const string &rid = "") {
    vector<uint8_t> extensions;
    auto add_ext = [&](uint8_t id, const vector<uint8_t> &data) {
        if (two_byte) {
            extensions.push_back(id);
            extensions.push_back(data.size());
        } else {
            extensions.push_back((id << 4) | (data.size() - 1));
        }
        extensions.insert(extensions.end(), data.begin(), data.end());
    };
    if (has_twcc) {
        add_ext(ext_id, {static_cast<uint8_t>(transport_seq >> 8), static_cast<uint8_t>(transport_seq)});
    }
    if (!rid.empty()) {
        add_ext(rtx ? 6 : 5, vector<uint8_t>(rid.begin(), rid.end()));
    }
    while (extensions.size() % 4) {
        extensions.push_back(0);
    }

    vector<uint8_t> bytes;
    bytes.push_back(extensions.empty() ? 0x80 : 0x90);
    bytes.push_back(rtx ? 97 : 96);
    put16(bytes, rtp_seq);
    put32(bytes, 90000);
    put32(bytes, ssrc);
    if (!extensions.empty()) {
        put16(bytes, two_byte ? 0x1000 : 0xbede);
        put16(bytes, extensions.size() / 4);
        bytes.insert(bytes.end(), extensions.begin(), extensions.end());
    }
    if (rtx) {
        put16(bytes, original_seq);
    }
    bytes.push_back(0x65);
    bytes.push_back(0x55);
    return bytes;
}

struct Feedback {
    vector<FCI_TWCC::TwccPacketStatus> reports;
    vector<uint32_t> sources;
    explicit Feedback(TwccContext &context) {
        context.setOnSendTwccCB([this](uint32_t ssrc, string data) {
            const auto *fci = reinterpret_cast<const FCI_TWCC *>(data.data());
            reports.push_back(fci->getPacketChunkList(data.size()));
            sources.push_back(ssrc);
        });
    }
    bool received(uint16_t sequence) const {
        for (const auto &report : reports) {
            const auto it = report.find(sequence);
            if (it != report.end() && it->second.first != SymbolStatus::not_received) {
                return true;
            }
        }
        return false;
    }
    bool missing(uint16_t sequence) const {
        for (const auto &report : reports) {
            const auto it = report.find(sequence);
            if (it != report.end() && it->second.first == SymbolStatus::not_received) {
                return true;
            }
        }
        return false;
    }
    int delta(uint16_t sequence) const {
        for (const auto &report : reports) {
            const auto it = report.find(sequence);
            if (it != report.end()) {
                return it->second.second;
            }
        }
        throw runtime_error("feedback sequence absent");
    }
};

struct TrackFixture {
    RtcMedia media;
    RtcCodecPlan primary;
    RtcCodecPlan repair;
    MediaTrack::Ptr track;
    unique_ptr<WrappedRtpTrack> rtp;
    unique_ptr<WrappedRtxTrack> rtx;
    uint32_t primary_ssrc;
    uint32_t rtx_ssrc;
    uint8_t ext_id;
    bool two_byte;

    TrackFixture(const shared_ptr<TestTransport> &transport, TwccContext &twcc,
        uint32_t ssrc = 1001, uint8_t extension_id = 7, bool two = false,
        bool negotiate_twcc = true) : primary_ssrc(ssrc), rtx_ssrc(ssrc + 1),
        ext_id(extension_id), two_byte(two) {
        media.type = TrackVideo;
        primary.pt = 96;
        primary.codec = "H264";
        primary.sample_rate = 90000;
        repair.pt = 97;
        repair.codec = "rtx";
        repair.sample_rate = 90000;
        auto add = [&](uint8_t id, RtpExtType type) {
            SdpAttrExtmap ext;
            ext.id = id;
            ext.ext = RtpExt::getExtUrl(type);
            media.extmap.emplace_back(std::move(ext));
        };
        if (negotiate_twcc) {
            add(ext_id, RtpExtType::transport_cc);
        }
        add(5, RtpExtType::sdes_rtp_stream_id);
        add(6, RtpExtType::sdes_repaired_rtp_stream_id);
        track = make_shared<MediaTrack>();
        track->media = &media;
        track->plan_rtp = &primary;
        track->plan_rtx = &repair;
        track->offer_ssrc_rtp = primary_ssrc;
        track->offer_ssrc_rtx = rtx_ssrc;
        track->rtp_ext_ctx = make_shared<RtpExtContext>(media);
        rtp.reset(new WrappedRtpTrack(track, twcc, *transport));
        rtx.reset(new WrappedRtxTrack(track, twcc));
    }

    void input(bool retransmit, uint16_t sequence, uint16_t transport_sequence,
        uint64_t time_ms, bool extension = true, const string &rid = "") {
        auto bytes = makePacket(retransmit, retransmit ? rtx_ssrc : primary_ssrc,
            retransmit ? 900 : sequence, sequence, transport_sequence,
            ext_id, extension, two_byte, rid);
        auto *header = reinterpret_cast<RtpHeader *>(bytes.data());
        auto *wrapper = retransmit ? static_cast<WrappedMediaTrack *>(rtx.get()) : rtp.get();
        wrapper->inputRtp(reinterpret_cast<char *>(bytes.data()), bytes.size(), time_ms, header);
    }
};

void testRepair(const EventPoller::Ptr &poller, bool two_byte, bool duplicate,
    bool extension, bool negotiate, bool rid) {
    auto transport = make_shared<TestTransport>(poller);
    TwccContext twcc;
    Feedback feedback(twcc);
    TrackFixture track(transport, twcc, 1001, two_byte ? 20 : 7, two_byte, negotiate);
    const string stream = rid ? "h" : "";
    if (rid) {
        track.track->offer_ssrc_rtp = 0; // RID can identify a source absent from SDP.
    }
    track.input(false, 10, 100, 10000, true, stream);
    // Original media seq 11 / transport seq 101 is lost.
    track.input(false, 12, 102, 10002, true, stream);
    track.input(true, 11, 103, 10003, extension, stream);
    if (duplicate) {
        track.input(true, 11, 103, 10004, extension, stream);
    }
    expect(transport->received.size() == 3, "RTX must fill the media gap before feedback is sent");
    expect(transport->received[1].seq == 11, "RTX original sequence was not restored");
    expect(transport->received[1].ssrc == track.primary_ssrc, "RTX primary SSRC was not restored");
    expect(transport->received[1].payload == string("\x65\x55", 2), "RTX OSN was not removed from payload");
    // Trigger the existing arrival-time-span threshold. No timer/flush API is added.
    track.input(false, 13, 104, 10300, true, stream);
    if (!negotiate) {
        expect(feedback.reports.empty(), "unnegotiated TWCC must not generate feedback");
        return;
    }
    expect(feedback.reports.size() == 1, "expected one existing-threshold feedback report");
    expect(feedback.received(100) && feedback.received(102) && feedback.received(104), "ordinary RTP feedback regressed");
    expect(feedback.missing(101), "RTX must not mark the lost original transmission as received");
    cout << "OBSERVATION original_transport_101=not_received rtx_transport_103="
         << (feedback.received(103) ? "received" : "not_received")
         << " restored_media_seq=" << transport->received[1].seq << endl;
    expect(feedback.received(103) == extension, "received RTX transport sequence missing from TWCC");
    if (extension) {
        expect(feedback.delta(103) == 4, "RTX arrival delta must be 1 ms (four 250us units)");
    }
}

void testRtxFirst(const EventPoller::Ptr &poller) {
    auto transport = make_shared<TestTransport>(poller);
    TwccContext twcc;
    Feedback feedback(twcc);
    TrackFixture track(transport, twcc);
    track.input(true, 9, 100, 10000);
    expect(transport->received.empty(), "RTX-before-primary must preserve upstream media drop behavior");
    track.input(false, 10, 101, 10001);
    track.input(false, 11, 102, 10300);
    expect(feedback.received(100), "RTX transport receipt must be recorded even before the media channel exists");
    expect(transport->received.size() == 2, "RTX-first changed media delivery");
}

void testWrap(const EventPoller::Ptr &poller) {
    auto transport = make_shared<TestTransport>(poller);
    TwccContext twcc;
    Feedback feedback(twcc);
    TrackFixture track(transport, twcc);
    track.input(false, 10, 65534, 10000);
    track.input(true, 11, 65535, 10001);
    track.input(false, 12, 0, 10002);
    track.input(false, 13, 1, 10300);
    expect(feedback.reports.size() == 2, "wrap should preserve existing two-report behavior");
    expect(feedback.received(65534) && feedback.received(65535)
        && feedback.received(0) && feedback.received(1), "RTX receipt lost across transport sequence wrap");
    expect(transport->received.size() == 4, "media forwarding regressed at TWCC wrap");
}

void testSharedTransport(const EventPoller::Ptr &poller) {
    auto transport = make_shared<TestTransport>(poller);
    TwccContext twcc;
    Feedback feedback(twcc);
    TrackFixture first(transport, twcc, 1001, 7);
    TrackFixture second(transport, twcc, 2001, 9);
    first.input(false, 10, 100, 10000);
    second.input(false, 20, 101, 10001);
    first.input(true, 11, 102, 10002);
    second.input(true, 21, 103, 10003);
    first.input(false, 12, 104, 10300);
    expect(feedback.reports.size() == 1, "tracks should share one TWCC context");
    for (uint16_t seq = 100; seq <= 104; ++seq) {
        expect(feedback.received(seq), "shared transport omitted an RTP or RTX arrival");
    }
    expect(transport->received.size() == 5, "shared transport media delivery regressed");
}

void testRtxTriggersFeedback(const EventPoller::Ptr &poller) {
    auto transport = make_shared<TestTransport>(poller);
    TwccContext twcc;
    Feedback feedback(twcc);
    TrackFixture track(transport, twcc);
    track.input(false, 10, 100, 10000);
    track.input(true, 11, 101, 10300);
    expect(feedback.reports.size() == 1, "RTX arrival should trigger the existing time-span threshold");
    expect(feedback.received(100) && feedback.received(101), "RTX-triggered feedback lost packet status");
    expect(feedback.sources[0] == track.rtx_ssrc, "feedback should use the received packet source, as ordinary RTP does");
}

} // namespace

int main() {
    EventPollerPool::setPoolSize(1);
    auto poller = EventPollerPool::Instance().getPoller();
    int passed = 0, failed = 0;
    poller->sync([&]() {
        auto run = [&](const string &name, const function<void()> &test) {
            try {
                test();
                ++passed;
                cout << "PASS " << name << endl;
            } catch (const exception &ex) {
                ++failed;
                cerr << "FAIL " << name << ": " << ex.what() << endl;
            }
        };
        run("one_byte_rtx_and_original_loss", [&]() { testRepair(poller, false, false, true, true, false); });
        run("two_byte_extension", [&]() { testRepair(poller, true, false, true, true, false); });
        run("duplicate_rtx_arrival", [&]() { testRepair(poller, false, true, true, true, false); });
        run("rtx_without_twcc_extension", [&]() { testRepair(poller, false, false, false, true, false); });
        run("twcc_not_negotiated", [&]() { testRepair(poller, false, false, true, false, false); });
        run("repaired_rid_without_offer_ssrc", [&]() { testRepair(poller, false, false, true, true, true); });
        run("rtx_before_primary", [&]() { testRtxFirst(poller); });
        run("transport_sequence_wrap", [&]() { testWrap(poller); });
        run("shared_transport_multiple_ssrcs", [&]() { testSharedTransport(poller); });
        run("rtx_triggers_existing_feedback_threshold", [&]() { testRtxTriggersFeedback(poller); });
    });
    cout << "RESULT passed=" << passed << " failed=" << failed << endl;
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
