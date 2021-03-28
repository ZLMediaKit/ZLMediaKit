//
// Created by xzl on 2021/3/27.
//

#include "Sdp.h"

using onCreateSdpItem = function<SdpItem::Ptr(const string &key, const string &value)>;
static unordered_map<string, onCreateSdpItem> sdpItemCreator;

template <typename Item>
void registerSdpItem(){
    onCreateSdpItem func = [](const string &key, const string &value) {
        auto ret = std::make_shared<Item>();
        ret->parse(value);
        return ret;
    };
    Item item;
    InfoL << "register sdp item:" << item.getKey();
    sdpItemCreator.emplace(item.getKey(), std::move(func));
}

static bool registerAllItem(){
    registerSdpItem<SdpString<'v'> >();
    registerSdpItem<SdpString<'s'> >();
    registerSdpItem<SdpString<'i'> >();
    registerSdpItem<SdpString<'u'> >();
    registerSdpItem<SdpString<'e'> >();
    registerSdpItem<SdpString<'p'> >();
    registerSdpItem<SdpString<'z'> >();
    registerSdpItem<SdpString<'k'> >();
    registerSdpItem<SdpString<'r'> >();
    registerSdpItem<SdpTime>();
    registerSdpItem<SdpOrigin>();
    registerSdpItem<SdpConnection>();
    registerSdpItem<SdpBandwidth>();
    registerSdpItem<SdpMedia>();
    registerSdpItem<SdpAttr>();
    registerSdpItem<SdpAttrGroup>();
    registerSdpItem<SdpAttrMsidSemantic>();
    registerSdpItem<SdpAttrRtcp>();
    registerSdpItem<SdpAttrIceUfrag>();
    registerSdpItem<SdpAttrIcePwd>();
    registerSdpItem<SdpAttrFingerprint>();
    registerSdpItem<SdpAttrSetup>();
    registerSdpItem<SdpAttrMid>();
    registerSdpItem<SdpAttrExtmap>();
    registerSdpItem<SdpAttrRtpMap>();
    registerSdpItem<SdpAttrRtcpFb>();
    registerSdpItem<SdpAttrFmtp>();
    registerSdpItem<SdpAttrSSRC>();
    registerSdpItem<SdpAttrSctpMap>();
    return true;
}

void RtcSdp::parse(const string &str) {
    static auto flag = registerAllItem();
    RtcMedia *media = nullptr;
    auto lines = split(str, "\n");
    for(auto &line : lines){
        trim(line);
        if(line.size() < 3 || line[1] != '='){
            continue;
        }
        auto key = line.substr(0, 1);
        auto value = line.substr(2);
        if (key == "m") {
            medias.emplace_back(RtcMedia());
            media = &medias.back();
        }

        SdpItem::Ptr item;
        auto it = sdpItemCreator.find(key);
        if (it != sdpItemCreator.end()) {
            item = it->second(key, value);
        } else {
            item = std::make_shared<SdpCommon>(key);
            item->parse(value);
        }
        if (media) {
            media->items.push_back(std::move(item));
        } else {
            items.push_back(std::move(item));
        }
    }
}

string RtcSdp::toString() const {
    return std::string();
}

void SdpAttr::parse(const string &str) {
    SdpItem::parse(str);
    auto pos = str.find(':');
    auto key = pos == string::npos ? str : str.substr(0, pos);
    auto value = pos == string::npos ? string() : str.substr(pos + 1);
    auto it = sdpItemCreator.find(key);
    if (it != sdpItemCreator.end()) {
        detail = it->second(key, value);
    } else {
        detail = std::make_shared<SdpCommon>(key);
        detail->parse(value);
    }
}

void test_sdp(){
    char str1[] = "v=0\n"
                 "o=- 380154348540553537 2 IN IP4 127.0.0.1\n"
                 "s=-\n"
                 "t=0 0\n"
                 "a=group:BUNDLE video\n"
                 "a=msid-semantic: WMS\n"
                 "m=video 9 RTP/SAVPF 96\n"
                 "c=IN IP4 0.0.0.0\n"
                 "a=rtcp:9 IN IP4 0.0.0.0\n"
                 "a=ice-ufrag:1ZFN\n"
                 "a=ice-pwd:70P3H0jPlGz1fiJl5XZfXMZH\n"
                 "a=ice-options:trickle\n"
                 "a=fingerprint:sha-256 3E:10:35:6B:9A:9E:B0:55:AC:2A:88:F5:74:C1:70:32:B5:8D:88:1D:37:B0:9C:69:A6:DD:07:10:73:27:1A:16\n"
                 "a=setup:active\n"
                 "a=mid:video\n"
                 "a=recvonly\n"
                 "a=rtcp-mux\n"
                 "a=rtpmap:96 H264/90000\n"
                 "a=fmtp:96 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f";
    char str2[] = "v=0\n"
                  "o=- 2584450093346841581 2 IN IP4 127.0.0.1\n"
                  "s=-\n"
                  "t=0 0\n"
                  "a=group:BUNDLE audio video data\n"
                  "a=msid-semantic: WMS 616cfbb1-33a3-4d8c-8275-a199d6005549\n"
                  "m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 126\n"
                  "c=IN IP4 0.0.0.0\n"
                  "a=rtcp:9 IN IP4 0.0.0.0\n"
                  "a=ice-ufrag:sXJ3\n"
                  "a=ice-pwd:yEclOTrLg1gEubBFefOqtmyV\n"
                  "a=fingerprint:sha-256 22:14:B5:AF:66:12:C7:C7:8D:EF:4B:DE:40:25:ED:5D:8F:17:54:DD:88:33:C0:13:2E:FD:1A:FA:7E:7A:1B:79\n"
                  "a=setup:actpass\n"
                  "a=mid:audio\n"
                  "a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\n"
                  "a=sendrecv\n"
                  "a=rtcp-mux\n"
                  "a=rtpmap:111 opus/48000/2\n"
                  "a=rtcp-fb:111 transport-cc\n"
                  "a=fmtp:111 minptime=10;useinbandfec=1\n"
                  "a=rtpmap:103 ISAC/16000\n"
                  "a=rtpmap:104 ISAC/32000\n"
                  "a=rtpmap:9 G722/8000\n"
                  "a=rtpmap:0 PCMU/8000\n"
                  "a=rtpmap:8 PCMA/8000\n"
                  "a=rtpmap:106 CN/32000\n"
                  "a=rtpmap:105 CN/16000\n"
                  "a=rtpmap:13 CN/8000\n"
                  "a=rtpmap:110 telephone-event/48000\n"
                  "a=rtpmap:112 telephone-event/32000\n"
                  "a=rtpmap:113 telephone-event/16000\n"
                  "a=rtpmap:126 telephone-event/8000\n"
                  "a=ssrc:120276603 cname:iSkJ2vn5cYYubTve\n"
                  "a=ssrc:120276603 msid:616cfbb1-33a3-4d8c-8275-a199d6005549 1da3d329-7399-4fe9-b20f-69606bebd363\n"
                  "a=ssrc:120276603 mslabel:616cfbb1-33a3-4d8c-8275-a199d6005549\n"
                  "a=ssrc:120276603 label:1da3d329-7399-4fe9-b20f-69606bebd363\n"
                  "m=video 9 UDP/TLS/RTP/SAVPF 96 98 100 102 127 97 99 101 125\n"
                  "c=IN IP4 0.0.0.0\n"
                  "a=rtcp:9 IN IP4 0.0.0.0\n"
                  "a=ice-ufrag:sXJ3\n"
                  "a=ice-pwd:yEclOTrLg1gEubBFefOqtmyV\n"
                  "a=fingerprint:sha-256 22:14:B5:AF:66:12:C7:C7:8D:EF:4B:DE:40:25:ED:5D:8F:17:54:DD:88:33:C0:13:2E:FD:1A:FA:7E:7A:1B:79\n"
                  "a=setup:actpass\n"
                  "a=mid:video\n"
                  "a=extmap:2 urn:ietf:params:rtp-hdrext:toffset\n"
                  "a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\n"
                  "a=extmap:4 urn:3gpp:video-orientation\n"
                  "a=extmap:5 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01\n"
                  "a=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay\n"
                  "a=sendrecv\n"
                  "a=rtcp-mux\n"
                  "a=rtcp-rsize\n"
                  "a=rtpmap:96 VP8/90000\n"
                  "a=rtcp-fb:96 ccm fir\n"
                  "a=rtcp-fb:96 nack\n"
                  "a=rtcp-fb:96 nack pli\n"
                  "a=rtcp-fb:96 goog-remb\n"
                  "a=rtcp-fb:96 transport-cc\n"
                  "a=rtpmap:98 VP9/90000\n"
                  "a=rtcp-fb:98 ccm fir\n"
                  "a=rtcp-fb:98 nack\n"
                  "a=rtcp-fb:98 nack pli\n"
                  "a=rtcp-fb:98 goog-remb\n"
                  "a=rtcp-fb:98 transport-cc\n"
                  "a=rtpmap:100 H264/90000\n"
                  "a=rtcp-fb:100 ccm fir\n"
                  "a=rtcp-fb:100 nack\n"
                  "a=rtcp-fb:100 nack pli\n"
                  "a=rtcp-fb:100 goog-remb\n"
                  "a=rtcp-fb:100 transport-cc\n"
                  "a=fmtp:100 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f\n"
                  "a=rtpmap:102 red/90000\n"
                  "a=rtpmap:127 ulpfec/90000\n"
                  "a=rtpmap:97 rtx/90000\n"
                  "a=fmtp:97 apt=96\n"
                  "a=rtpmap:99 rtx/90000\n"
                  "a=fmtp:99 apt=98\n"
                  "a=rtpmap:101 rtx/90000\n"
                  "a=fmtp:101 apt=100\n"
                  "a=rtpmap:125 rtx/90000\n"
                  "a=fmtp:125 apt=102\n"
                  "a=ssrc-group:FID 2580761338 611523443\n"
                  "a=ssrc:2580761338 cname:iSkJ2vn5cYYubTve\n"
                  "a=ssrc:2580761338 msid:616cfbb1-33a3-4d8c-8275-a199d6005549 bf270496-a23e-47b5-b901-ef23096cd961\n"
                  "a=ssrc:2580761338 mslabel:616cfbb1-33a3-4d8c-8275-a199d6005549\n"
                  "a=ssrc:2580761338 label:bf270496-a23e-47b5-b901-ef23096cd961\n"
                  "a=ssrc:611523443 cname:iSkJ2vn5cYYubTve\n"
                  "a=ssrc:611523443 msid:616cfbb1-33a3-4d8c-8275-a199d6005549 bf270496-a23e-47b5-b901-ef23096cd961\n"
                  "a=ssrc:611523443 mslabel:616cfbb1-33a3-4d8c-8275-a199d6005549\n"
                  "a=ssrc:611523443 label:bf270496-a23e-47b5-b901-ef23096cd961\n"
                  "a=candidate:1 1 udp %u %s %u typ host\n"
                  "m=application 9 DTLS/SCTP 5000\n"
                  "c=IN IP4 0.0.0.0\n"
                  "a=ice-ufrag:sXJ3\n"
                  "a=ice-pwd:yEclOTrLg1gEubBFefOqtmyV\n"
                  "a=fingerprint:sha-256 22:14:B5:AF:66:12:C7:C7:8D:EF:4B:DE:40:25:ED:5D:8F:17:54:DD:88:33:C0:13:2E:FD:1A:FA:7E:7A:1B:79\n"
                  "a=setup:actpass\n"
                  "a=mid:data\n"
                  "a=sctpmap:5000 webrtc-datachannel 1024";

    RtcSdp sdp1;
    sdp1.parse(str1);

    RtcSdp sdp2;
    sdp2.parse(str2);

    InfoL << sdp1.toString();
    InfoL << sdp2.toString();
}
