/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "ext-codec/VP9.h"
#include "ext-codec/VP9Rtp.h"

using namespace mediakit;

namespace {

struct ParseCase {
    const char *name;
    std::vector<unsigned char> packet;
    int expected_offset;
};

void require(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << message << std::endl;
        std::exit(1);
    }
}

void runCase(const ParseCase &test) {
    VP9RtpDecoder decoder;
    std::vector<std::string> frames;
    decoder.addDelegate([&](const Frame::Ptr &frame) {
        frames.emplace_back(frame->data(), frame->size());
        return true;
    });

    auto packet = test.packet;
    packet[0] |= 0x0C; // B/E: make a successful descriptor dispatch one complete frame.
    RtpInfo rtp_info(0x12345678, 1400, 90000, 98, 0, 0);
    auto rtp = rtp_info.makeRtp(TrackVideo, packet.data(), packet.size(), true, 100);
    require(rtp != nullptr, std::string(test.name) + ": failed to create RTP packet");
    decoder.inputRtp(rtp, false);

    const bool should_output = test.expected_offset >= 0;
    require(frames.size() == (should_output ? 1U : 0U),
            std::string(test.name) + ": unexpected decoded frame count " + std::to_string(frames.size()));
    if (should_output) {
        require(frames[0].size() == packet.size() - static_cast<size_t>(test.expected_offset),
                std::string(test.name) + ": decoded frame has an unexpected size");
        require(frames[0] == std::string(reinterpret_cast<const char *>(packet.data() + test.expected_offset),
                                        packet.size() - static_cast<size_t>(test.expected_offset)),
                std::string(test.name) + ": decoded frame payload does not match RTP frame payload");
    }
}

void testParseFailureDropsCurrentFrame(const char *name, const std::vector<unsigned char> &invalid_payload) {
    VP9RtpDecoder decoder;
    std::vector<std::string> frames;
    decoder.addDelegate([&](const Frame::Ptr &frame) {
        frames.emplace_back(frame->data(), frame->size());
        return true;
    });

    RtpInfo rtp_info(0x12345678, 1400, 90000, 98, 0, 0);
    auto input = [&](const std::vector<unsigned char> &payload, bool mark) {
        auto rtp = rtp_info.makeRtp(TrackVideo, payload.data(), payload.size(), mark, 100);
        require(rtp != nullptr, std::string(name) + ": failed to create RTP packet");
        decoder.inputRtp(rtp, false);
    };

    input({ 0x08, 0x80, 0x11 }, false); // B: begin assembling a frame.
    input(invalid_payload, false);
    input({ 0x04, 0x22 }, true);        // E: must not emit the incomplete frame.
    require(frames.empty(), std::string(name) + ": parse failure emitted a partial VP9 frame");

    input({ 0x0C, 0x80, 0x33 }, true); // A new complete frame must recover normally.
    require(frames.size() == 1, std::string(name) + ": decoder did not recover after a parse failure");
    require(frames[0] == std::string("\x80\x33", 2),
            std::string(name) + ": recovered VP9 frame payload does not match");
}

void testParseFailureDropsGop() {
    VP9RtpDecoder decoder;
    std::vector<std::string> frames;
    decoder.addDelegate([&](const Frame::Ptr &frame) {
        frames.emplace_back(frame->data(), frame->size());
        return true;
    });

    RtpInfo rtp_info(0x12345678, 1400, 90000, 98, 0, 0);
    auto input = [&](const std::vector<unsigned char> &payload) {
        auto rtp = rtp_info.makeRtp(TrackVideo, payload.data(), payload.size(), true, 100);
        require(rtp != nullptr, "failed to create RTP packet for GOP recovery test");
        decoder.inputRtp(rtp, false);
    };

    input({ 0x50, 0x03 });             // Reject a truncated P_DIFF chain.
    input({ 0x4C, 0x00, 0x44 });       // Suppress a dependent P-frame.
    require(frames.empty(), "parse failure did not suppress the damaged GOP");

    input({ 0x0C, 0x80, 0x55 });       // A keyframe starts a new GOP.
    input({ 0x4C, 0x00, 0x66 });       // Its dependent P-frame is valid again.
    require(frames.size() == 2, "decoder did not recover at the next VP9 keyframe");
    require(frames[0] == std::string("\x80\x55", 2), "recovery keyframe payload does not match");
    require(frames[1] == std::string("\x00\x66", 2), "recovered P-frame payload does not match");
}

void testFrameKeyFrameDetection() {
    VP9Frame key_frame;
    key_frame._buffer.assign("\x80", 1);
    require(key_frame.keyFrame(), "VP9 keyframe was not recognized");

    VP9Frame inter_frame;
    inter_frame._buffer.assign("\x84", 1);
    require(!inter_frame.keyFrame(), "VP9 inter frame was incorrectly recognized as a keyframe");

    VP9Frame shown_frame;
    shown_frame._buffer.assign("\x88", 1);
    require(!shown_frame.keyFrame(), "VP9 show-existing frame was incorrectly recognized as a keyframe");

    VP9Frame invalid_frame;
    invalid_frame._buffer.assign("\x00", 1);
    require(!invalid_frame.keyFrame(), "VP9 frame with an invalid marker was incorrectly recognized as a keyframe");
}

} // namespace

int main() {
    // The last byte in every successful case is frame payload. These cases cover
    // all L/F/P combinations and make incorrect descriptor consumption visible.
    const std::vector<ParseCase> combinations = {
        { "L0 F0 P0", { 0x00, 0xAA }, 1 },
        { "L0 F0 P1", { 0x40, 0xAA }, 1 },
        { "L0 F1 P0", { 0x10, 0xAA }, 1 },
        { "L0 F1 P1", { 0x50, 0x02, 0xAA }, 2 },
        { "L1 F0 P0", { 0x20, 0x00, 0x33, 0xAA }, 3 },
        { "L1 F0 P1", { 0x60, 0x00, 0x33, 0xAA }, 3 },
        { "L1 F1 P0 does not consume P_DIFF", { 0x30, 0x00, 0xAA }, 2 },
        { "L1 F1 P1 consumes P_DIFF once", { 0x70, 0x00, 0x02, 0xAA }, 3 },
    };
    for (const auto &test : combinations) {
        runCase(test);
    }

    runCase({ "three references", { 0x50, 0x03, 0x05, 0x06, 0xAA }, 4 });
    runCase({ "truncated reference chain", { 0x50, 0x03 }, -1 });
    runCase({ "more than three references", { 0x50, 0x03, 0x05, 0x07, 0x08, 0xAA }, -1 });
    runCase({ "short picture ID", { 0x80, 0x12, 0xAA }, 2 });
    runCase({ "large picture ID", { 0x80, 0x81, 0x23, 0xAA }, 3 });
    runCase({ "truncated large picture ID", { 0x80, 0x81 }, -1 });
    runCase({ "resolution", { 0x02, 0x10, 0x07, 0x80, 0x04, 0x38, 0xAA }, 6 });
    runCase({ "truncated resolution", { 0x02, 0x10, 0x07, 0x80, 0x04 }, -1 });

    // V=1, G=1, N_G=2. Each GOF frame byte is followed immediately by
    // exactly the number of reference indices it declares.
    runCase({ "GOF exact boundary", { 0x02, 0x08, 0x02, 0x04, 0x11, 0x08, 0x22, 0x33, 0xAA }, 8 });
    runCase({ "GOF truncated references", { 0x02, 0x08, 0x01, 0x08, 0x22 }, -1 });

    runCase({ "reject descriptor-only payload", { 0x00 }, -1 });
    runCase({ "reject layer-descriptor-only payload", { 0x30, 0x00 }, -1 });
    testParseFailureDropsCurrentFrame("truncated P_DIFF", { 0x50, 0x03 });
    testParseFailureDropsCurrentFrame("descriptor-only middle packet", { 0x00 });
    testParseFailureDropsGop();
    testFrameKeyFrameDetection();
    return 0;
}
