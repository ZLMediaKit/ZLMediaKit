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

// The Location header tells the client to DELETE this exact request target. Feed it through the same
// request parser the http api layer uses, then check that the id/token stay percent-encoded until the
// handler decodes them.
void testDeleteWebrtcLocationRoundTripViaHttpRequest() {
    const string raw_id = "Ab+/9";
    const string raw_token = "token+/9";

    HttpArgs args;
    args["id"] = raw_id;
    args["token"] = raw_token;
    auto request = "DELETE /index/api/delete_webrtc?" + args.make() + " HTTP/1.1\r\nHost: localhost\r\n\r\n";

    Parser parser;
    parser.parse(request.data(), request.size());

    expect(parser.method() == "DELETE", "request method should be parsed as DELETE");
    expect(parser.url() == "/index/api/delete_webrtc", "request url should be parsed without the query string");

    // getAllArgs()/getUrlArgs() hand these over as-is, without percent-decoding.
    auto &url_args = parser.getUrlArgs();
    expect(url_args["id"] == "Ab%2B%2F9", "url arg should still be percent-encoded after request parsing");
    expect(url_args["id"] != raw_id, "url arg must not be assumed to be already decoded");
    expect(url_args["token"] == "token%2B%2F9", "token should still be percent-encoded after request parsing");

    // delete_webrtc decodes id/token before looking up the transport; that is what makes %2F match '/'.
    expect(strCoding::UrlDecodeComponent(url_args["id"]) == raw_id,
           "decoding the url arg should recover the transport id");
    expect(strCoding::UrlDecodeComponent(url_args["token"]) == raw_token,
           "decoding the url arg should recover the token");
}

} // namespace

int main() {
    try {
        testBundleOnlyDatachannelAnswer();
        testDeleteWebrtcLocationQueryRoundTrip();
        testDeleteWebrtcLocationRoundTripViaHttpRequest();
        cout << "test_webrtc_regression passed" << endl;
        return 0;
    } catch (const exception &ex) {
        cerr << "test_webrtc_regression failed: " << ex.what() << endl;
        return EXIT_FAILURE;
    }
}
