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
#include <stdexcept>
#include <string>

#include "Common/Parser.h"
#include "Common/strCoding.h"
#include "Http/HttpClient.h"
#include "../webrtc/Sdp.h"

using namespace std;
using namespace mediakit;

namespace {

void expect(bool cond, const string &msg) {
    if (!cond) {
        throw runtime_error(msg);
    }
}

string makeBundleOnlyDatachannelOffer() {
    return
        "v=0\r\n"
        "o=- 0 0 IN IP4 127.0.0.1\r\n"
        "s=-\r\n"
        "t=0 0\r\n"
        "a=group:BUNDLE 0\r\n"
        "a=msid-semantic: WMS\r\n"
        "m=application 0 UDP/DTLS/SCTP webrtc-datachannel\r\n"
        "c=IN IP4 0.0.0.0\r\n"
        "a=ice-ufrag:remoteUfrag\r\n"
        "a=ice-pwd:remotePassword1234567890\r\n"
        "a=fingerprint:sha-256 "
        "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:"
        "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF\r\n"
        "a=setup:actpass\r\n"
        "a=mid:0\r\n"
        "a=bundle-only\r\n"
        "a=sctp-port:5000\r\n";
}

void testBundleOnlyDatachannelAnswer() {
    RtcSession offer;
    offer.loadFrom(makeBundleOnlyDatachannelOffer());
    offer.checkValid();

    expect(offer.media.size() == 1, "offer should contain a single application m-line");
    expect(offer.media[0].bundle_only, "offer application m-line should preserve a=bundle-only");

    SdpAttrFingerprint local_fingerprint;
    local_fingerprint.algorithm = "sha-256";
    local_fingerprint.hash =
        "FF:EE:DD:CC:BB:AA:99:88:77:66:55:44:33:22:11:00:"
        "FF:EE:DD:CC:BB:AA:99:88:77:66:55:44:33:22:11:00";

    RtcConfigure configure;
    configure.setDefaultSetting("localUfrag", "localPassword1234567890", RtpDirection::sendrecv, local_fingerprint);

    auto answer = configure.createAnswer(offer);
    expect(answer != nullptr, "createAnswer should return a session");
    expect(answer->media.size() == 1, "answer should contain a single application m-line");

#ifdef ENABLE_SCTP
    answer->checkValid();
    expect(answer->media[0].port == 9, "bundle-only application m-line should use port 9 in answer");
    expect(answer->group.mids.size() == 1 && answer->group.mids[0] == "0",
           "accepted bundle-only application m-line should remain in group:BUNDLE");
#else
    expect(answer->media[0].port == 0, "application m-line should stay rejected when SCTP is disabled");
#endif
}

void testDeleteWebrtcLocationQueryRoundTrip() {
    const string raw_id = "Ab+/9";
    const string raw_token = "token+/9";

    HttpArgs args;
    args["id"] = raw_id;
    args["token"] = raw_token;
    auto query = args.make();

    expect(query.find("id=Ab%2B%2F9") != string::npos, "id should be URL-encoded in delete_webrtc query");
    expect(query.find("token=token%2B%2F9") != string::npos,
           "token should be URL-encoded in delete_webrtc query");

    auto parsed = Parser::parseArgs(query);
    expect(strCoding::UrlDecodeComponent(parsed["id"]) == raw_id, "encoded id should round-trip through query parsing");
    expect(strCoding::UrlDecodeComponent(parsed["token"]) == raw_token,
           "encoded token should round-trip through query parsing");
}

// 源流 SDP 中 a=fmtp:<pt> 不带参数(部分摄像机/NVR 的 SDP 如此)时，应视为没有 fmtp 参数，而不是解析失败
// a=fmtp:<pt> without parameters (as some cameras/NVRs write it) means "no fmtp" rather than a parse error
void testFmtpWithoutParameters() {
    // issue #4863 中真实摄像机的 DESCRIBE SDP
    // The real camera DESCRIBE SDP from issue #4863
    const string camera_sdp =
        "v=0\r\n"
        "o=- 1109162014219182 1109162014219192 IN IP4 127.0.0.1\r\n"
        "s=Media Presentation\r\n"
        "c=IN IP4 0.0.0.0\r\n"
        "t=0 0\r\n"
        "a=range:npt=now-\r\n"
        "m=video 0 RTP/AVP 96\r\n"
        "a=rtpmap:96 H264/90000\r\n"
        "a=fmtp:96\r\n"
        "a=control:trackID=1\r\n";

    RtcSession session;
    session.loadFrom(camera_sdp);

    auto media = session.getMedia(TrackType::TrackVideo);
    expect(media != nullptr, "video m-line should be parsed");
    expect(media->plan.size() == 1, "video m-line should have one codec plan");
    expect(media->plan[0].pt == 96, "payload type should be 96");
    expect(media->plan[0].codec == "H264", "codec should be H264");
    expect(media->plan[0].fmtp.empty(), "a=fmtp without parameters should yield an empty fmtp map");

    // 带参数的 fmtp 行为必须保持不变
    // fmtp with parameters must keep working as before
    RtcSession with_params;
    with_params.loadFrom(
        "v=0\r\no=- 1 2 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\n"
        "m=video 0 RTP/AVP 96\r\na=rtpmap:96 H264/90000\r\n"
        "a=fmtp:96 packetization-mode=1;profile-level-id=420029\r\n");

    auto media2 = with_params.getMedia(TrackType::TrackVideo);
    expect(media2 != nullptr, "video m-line should be parsed");
    expect(media2->plan.size() == 1, "video m-line should have one codec plan");
    expect(media2->plan[0].fmtp.size() == 2, "two fmtp parameters should be kept");
    expect(media2->plan[0].getFmtp("packetization-mode") == "1", "packetization-mode should be kept");
    expect(media2->plan[0].getFmtp("profile-level-id") == "420029", "profile-level-id should be kept");
}

} // namespace

int main() {
    try {
        testBundleOnlyDatachannelAnswer();
        testDeleteWebrtcLocationQueryRoundTrip();
        testFmtpWithoutParameters();
        cout << "test_webrtc_regression passed" << endl;
        return 0;
    } catch (const exception &ex) {
        cerr << "test_webrtc_regression failed: " << ex.what() << endl;
        return EXIT_FAILURE;
    }
}
