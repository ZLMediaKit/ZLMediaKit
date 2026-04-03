/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <iomanip>
#include <random>
#include "SoapUtil.h"
#include "Util/util.h"
#include "Util/SHA1.h"
#include "Util/logger.h"
#include "Util/base64.h"
#include "Util/onceToken.h"
#include "Http/HttpRequester.h"
#include "Rtsp/Rtsp.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

static pugi::xml_node find_node(const pugi::xml_node &parent, const std::string &end_str) {
    auto ret = parent.find_child([&](const pugi::xml_node &node) {
        auto len = strlen(node.name());
        if (len < end_str.size()) {
            return false;
        }
        if (end_str == node.name()) {
            return true;
        }
        if (*(node.name() + len - end_str.size() - 1) != ':') {
            return false;
        }
        return strcasecmp(node.name() + len - end_str.size(), end_str.data()) == 0;
    });
    return ret;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

SoapObject::SoapObject() {
    _root = std::make_shared<pugi::xml_node>();
}

SoapObject::SoapObject(std::shared_ptr<pugi::xml_node> node) {
    _root = std::move(node);
}

void SoapObject::load(const char *data, size_t len) {
    auto doc = std::make_shared<pugi::xml_document>();
    auto result = doc->load_string(data, len);
    if (!result) {
        throw std::invalid_argument(string("解析xml失败:") + result.description());
    }
    _root = std::move(doc);
}

SoapObject::operator bool() const {
    return !_root->empty();
}

SoapObject SoapObject::operator[](const string &path) const{
    auto hit = *_root;
    auto node_name = split(path, "/");
    for (auto &node : node_name) {
        hit = find_node(hit, node);
        if (hit.empty()) {
            return SoapObject();
        }
    }
    auto ref = _root;
    shared_ptr<pugi::xml_node> node(new pugi::xml_node(std::move(hit)), [ref](pugi::xml_node *ptr) {
        delete ptr;
    });
    return SoapObject(std::move(node));
}

SoapObject SoapObject::operator[](size_t index) const {
    for (auto &hit : *_root) {
        if (index-- == 0) {
            auto ref = _root;
            shared_ptr<pugi::xml_node> node(new pugi::xml_node(hit), [ref](pugi::xml_node *ptr) {
                delete ptr;
            });
            return SoapObject(node);
        }
    }
    return SoapObject();
}

std::string SoapObject::as_string() const {
    if (!(bool) (*this)) {
        return "";
    }
    xml_string_writer writer;
    _root->print(writer);
    return writer.result;
}

pugi::xml_node SoapObject::as_xml() const {
    return *_root;
}

SoapObject::SoapObject(const pugi::xml_node &node, const SoapObject &ref) {
    auto root = ref._root;
    _root.reset(new pugi::xml_node(node.internal_object()), [root](pugi::xml_node *ptr) {
        delete ptr;
    });
}

std::string SoapUtil::createDiscoveryString(const string &uuid_in) {
    auto uuid = uuid_in;
    if (uuid.empty()) {
        uuid = createUuidString();
    }
    static constexpr char str_fmt[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                      "<e:Envelope xmlns:e=\"http://www.w3.org/2003/05/soap-envelope\"\n"
                                      "            xmlns:w=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\"\n"
                                      "            xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\"\n"
                                      "            xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\">\n"
                                      "    <e:Header>\n"
                                      "        <w:MessageID>%s</w:MessageID>\n"
                                      "        <w:To e:mustUnderstand=\"true\">urn:schemas-xmlsoap-org:ws:2005:04:discovery</w:To>\n"
                                      "        <w:Action a:mustUnderstand=\"true\">http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</w:Action>\n"
                                      "    </e:Header>\n"
                                      "    <e:Body>\n"
                                      "        <d:Probe>\n"
                                      "            <d:Types>dn:NetworkVideoTransmitter</d:Types>\n"
                                      "        </d:Probe>\n"
                                      "    </e:Body>\n"
                                      "</e:Envelope>";
    return print_to_string(str_fmt, uuid.data());
}

static std::string creatString4() {
    std::mt19937 rng(std::random_device{}());
    string ret = StrPrinter << std::hex << std::uppercase << std::setfill('0') << ((1 + rng()) & 0xFFFF);
    return ret;
}

std::string SoapUtil::createUuidString() {
    auto ret = std::string("uuid:") +
               creatString4() + creatString4() + '-' +
               creatString4() + '-' +
               creatString4() + '-' +
               creatString4() + '-' +
               creatString4() + creatString4() + creatString4();
    TraceL << ret;
    return ret;
}

static tuple<string/*passdigest*/, string/*nonce*/, string/*timestamp*/>
makePasswordDigest(const string &user_name, const string &passwd) {
    std::mt19937 rng(std::random_device{}());
    string nonce;
    nonce.resize(16);
    for (auto &ch : nonce) {
        ch = rng() & 0xFF;
    }
    auto timestamp = getTimeStr("%Y-%m-%dT%H:%M:%S%z");
    auto passdigest = SHA1::encode_bin(nonce + timestamp + passwd);
    return std::make_tuple(encodeBase64(passdigest), encodeBase64(nonce), timestamp);
}

static std::string
createSoapSecurity(const string &user_name, const string &passdigest, const string &nonce, const string &timestamp) {
    static constexpr char str_fmt[] =
            R"(<Security s:mustUnderstand="1" xmlns="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd">
            <UsernameToken>
            <Username>%s</Username>
            <Password Type="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest">%s</Password>
            <Nonce EncodingType="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary">%s</Nonce>
            <Created xmlns="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd">%s</Created>
            </UsernameToken></Security>)";
    return print_to_string(str_fmt, user_name.data(), passdigest.data(), nonce.data(), timestamp.data());
}

std::string SoapUtil::createSoapRequest(const string &body, const string &user_name, const string &passwd) {
    string header = R"(<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" xmlns:a="http://www.w3.org/2005/08/addressing"><s:Header>)";
    if (!user_name.empty() && !passwd.empty()) {
        auto req = makePasswordDigest(user_name, passwd);
        header += createSoapSecurity(user_name, std::get<0>(req), std::get<1>(req), std::get<2>(req));
    }
    header += R"(</s:Header><s:Body xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema">)";
    header += body;
    header += R"(</s:Body></s:Envelope>)";
    return header;
}

SoapErr::SoapErr(std::string url,
                 std::string action,
                 SockException ex,
                 const mediakit::Parser &parser,
                 std::string err) {
    _url = std::move(url);
    _action = std::move(action);
    _net_err = std::move(ex);
    _http_code = atoi(parser.status().data());
    _http_msg = parser.statusStr();
    _other_err = std::move(err);
}

SoapErr::operator std::string() const {
    _StrPrinter printer;
    printer << "request onvif service failed, url:" << _url << ", action:" << _action << ", ";
    if (_net_err) {
        return printer << "network err:" << _net_err.what() << endl;
    }
    if (_http_code != 200) {
        return printer << "http bad status:" << _http_code << " " << _http_msg << endl;
    }
    if (!_other_err.empty()) {
        return printer << _other_err << endl;
    }
    return "";
}

SoapErr::operator bool() const {
    return _net_err || _http_code != 200 || !_other_err.empty();
}

bool SoapErr::empty() const {
    return !*this;
}

int SoapErr::httpCode() const {
    return _http_code;
}

std::ostream& operator<<(std::ostream& sout, const SoapErr &err) {
    sout << (string)err;
    return sout;
}

void SoapUtil::sendSoapRequest(const string &url, const string &SOAPAction, const string &body, const SoapRequestCB &func,
                               float timeout_sec) {
    HttpRequester::Ptr requester(new HttpRequester);
    requester->setMethod("POST");
    requester->setBody(body);
    requester->addHeader("Content-Type", "text/xml; charset=utf-8; action=" + SOAPAction);
    requester->addHeader("Accept", "text/xml; charset=utf-8");
    requester->addHeader("SOAPAction", SOAPAction);
    std::shared_ptr<Ticker> ticker(new Ticker);
    requester->startRequester(url, [url, SOAPAction, func, requester, ticker](const SockException &ex,
                                                                              const Parser &parser) mutable {

        onceToken token(nullptr, [&]() mutable {
            requester.reset();
        });
        auto invoker = [&](const SoapObject &node, const SoapErr &err) {
            if (err) {
                WarnL << err;
            }
            if (func) {
                func(node, err);
            }
        };

        if (ex) {
            invoker(SoapObject(), SoapErr(url, SOAPAction, ex, parser));
            return;
        }
        if (parser.status() != "200") {
            invoker(SoapObject(), SoapErr(url, SOAPAction, ex, parser));
            return;
        }
        SoapObject root;
        try {
            root.load(parser.content().data(), parser.content().size());
        } catch (std::exception &e) {
            auto err = StrPrinter << "[parse xml failed]:" << e.what() << endl;
            invoker(SoapObject(), SoapErr(url, SOAPAction, ex, parser, err));
            return;
        }
        auto body = root["Envelope/Body"];
        if (!body) {
            auto err = StrPrinter << "[invalid onvif soap response]:" << ex.what() << endl;
            invoker(SoapObject(), SoapErr(url, SOAPAction, ex, parser, err));
            return;
        }
        auto fault = body["Fault"];
        if (fault) {
            auto err = StrPrinter << "[onvif soap fault]:" << fault["Reason/Text"].as_xml().text().as_string() << endl;;
            invoker(SoapObject(), SoapErr(url, SOAPAction, ex, parser, err));
            return;
        }
        //成功
        invoker(body, SoapErr(url, SOAPAction, ex, parser));
    }, timeout_sec);
}

void SoapUtil::sendGetDeviceInformation(const std::string &device_service, const std::string &user_name,
                                        const std::string &pwd, SoapRequestCB cb) {
    static constexpr char action_url[] = R"("http://www.onvif.org/ver10/device/wsdl/GetDeviceInformation")";
    static constexpr char str_fmt[] = R"(<GetDeviceInformation xmlns="http://www.onvif.org/ver10/device/wsdl"/>)";
    auto body = SoapUtil::createSoapRequest(str_fmt, user_name, pwd);
    SoapUtil::sendSoapRequest(device_service, action_url, body, std::move(cb));
}

void SoapUtil::sendGetProfiles(bool is_media2, const string &media_url, const string &user_name, const string &pwd,
                               const onGetProfilesResponse &cb) {
    auto invoker = [is_media2, cb](const SoapObject &res, const SoapErr &err) {
        if (err) {
            cb(err, vector<onGetProfilesResponseTuple>());
            return;
        }
        multimap<int, onGetProfilesResponseTuple> sorted;
        for (auto &xml_node : res["GetProfilesResponse"].as_xml()) {
            SoapObject obj(xml_node, res);
            auto profile_name = obj["Name"].as_xml().text().as_string();
            auto token = xml_node.attribute("token");
            if (token) {
                profile_name = token.value();
            }
            auto codec = obj[string(is_media2 ? "Configurations/VideoEncoder/Encoding"
                                              : "VideoEncoderConfiguration/Encoding")].as_xml().text().as_string();
            auto width = obj[string(is_media2 ? "Configurations/VideoEncoder/Resolution/Width"
                                              : "VideoEncoderConfiguration/Resolution/Width")].as_xml().text().as_int();
            auto height = obj[string(is_media2 ? "Configurations/VideoEncoder/Resolution/Height"
                                               : "VideoEncoderConfiguration/Resolution/Height")].as_xml().text().as_int();
            sorted.emplace(width * height, std::make_tuple(profile_name, codec, width, height));
        }
        vector<onGetProfilesResponseTuple> result;
        for (auto &pr : sorted) {
            result.insert(result.begin(), pr.second);
        }
        cb(err, result);
    };
    static constexpr char action_url[] = R"("http://www.onvif.org/ver10/media/wsdl/GetProfiles")";
    static constexpr char str_fmt[] = R"(<GetProfiles xmlns="http://www.onvif.org/ver10/media/wsdl"/>)";
    static constexpr char action_url2[] = R"("http://www.onvif.org/ver20/media/wsdl/GetProfiles")";
    static constexpr char str_fmt2[] = R"(<GetProfiles xmlns="http://www.onvif.org/ver20/media/wsdl"><Type>All</Type></GetProfiles>)";

    auto body = SoapUtil::createSoapRequest(is_media2 ? str_fmt2 : str_fmt, user_name, pwd);
    SoapUtil::sendSoapRequest(media_url, is_media2 ? action_url2 : action_url, body, invoker);
}

void SoapUtil::sendGetServices(const std::string &device_service, const std::initializer_list<std::string> &ns_filter,
                               const std::string &user_name, const std::string &pwd, const onGetServicesResponse &cb) {
    static constexpr char action_url[] = R"("http://www.onvif.org/ver10/device/wsdl/GetServices")";
    static constexpr char str_fmt[] = R"(<GetServices xmlns="http://www.onvif.org/ver10/device/wsdl">
                                            <IncludeCapability>true</IncludeCapability>
                                        </GetServices>)";
    set<string, StrCaseCompare> filter = ns_filter;
    auto body = SoapUtil::createSoapRequest(str_fmt, user_name, pwd);
    SoapUtil::sendSoapRequest(device_service, action_url, body,[filter, cb](const SoapObject &node, const SoapErr &err) {
        onGetServicesResponseMap mp;
        if (err) {
            cb(err, mp);
            return;
        }
        auto res = node["GetServicesResponse"];
        for (auto &xml_node : res.as_xml()) {
            SoapObject obj(xml_node, node);
            string ns = obj["Namespace"].as_xml().text().as_string();
            string xaddr = obj["XAddr"].as_xml().text().as_string();
            if (filter.find(ns) != filter.end()) {
                mp.emplace(ns, xaddr);
            }
        }
        cb(err, mp);
    });
}

static string getRtspUrlWithAuth(const std::string &user_name, const std::string &pwd, const string &url) {
    RtspUrl parser;
    parser.parse(url);
    if (user_name.empty() || pwd.empty() || !parser._user.empty() || !parser._passwd.empty()) {
        return url;
    }
    auto ret = url;
    auto pos = ret.find("://");
    if (pos == string::npos) {
        return ret;
    }
    ret.insert(pos + 3, (user_name + ":" + pwd + "@").data());
    return ret;
}

void SoapUtil::sendGetStreamUri(bool is_media2, const string &media_url, const string &profile,
                                const std::string &user_name, const std::string &pwd,
                                const onGetStreamUriResponse &cb) {
    if (!is_media2) {
        static constexpr char action_url[] = R"("http://www.onvif.org/ver10/media/wsdl/GetStreamUri")";
        static constexpr char str_fmt[] = R"(<GetStreamUri xmlns="http://www.onvif.org/ver10/media/wsdl">
                                              <StreamSetup>
                                                <Stream xmlns="http://www.onvif.org/ver10/schema">%s</Stream>
                                                <Transport xmlns="http://www.onvif.org/ver10/schema">
                                                  <Protocol>%s</Protocol>
                                                </Transport>
                                              </StreamSetup>
                                              <ProfileToken>%s</ProfileToken>
                                            </GetStreamUri>)";

        auto body = SoapUtil::createSoapRequest(print_to_string(str_fmt, "RTP-Unicast", "RTSP", profile.data()),
                                                user_name, pwd);
        SoapUtil::sendSoapRequest(media_url, action_url, body, [cb](const SoapObject &node, const SoapErr &err) {
            if (err) {
                cb(err, "");
                return;
            }
            auto res = node["GetStreamUriResponse/MediaUri/Uri"];
            cb(err, res.as_xml().text().as_string());
        });
    } else {
        static constexpr char action_url[] = R"("http://www.onvif.org/ver20/media/wsdl/GetStreamUri")";
        static constexpr char str_fmt[] = R"(<GetStreamUri xmlns="http://www.onvif.org/ver20/media/wsdl">
                                                <Protocol>%s</Protocol>
                                                <ProfileToken>%s</ProfileToken>
                                            </GetStreamUri>)";
        auto body = SoapUtil::createSoapRequest(print_to_string(str_fmt, "RTSP", profile.data()), user_name, pwd);
        SoapUtil::sendSoapRequest(media_url, action_url, body, [cb](const SoapObject &node, const SoapErr &err) {
            if (err) {
                cb(err, "");
                return;
            }
            auto res = node["GetStreamUriResponse/Uri"];
            cb(err, res.as_xml().text().as_string());
        });
    }
}

static void asyncGetStreamUri_l(const std::string &onvif_url, const std::string &user_name,
                                const std::string &pwd, const SoapUtil::AsyncGetStreamUriCB &cb,
                                std::shared_ptr<int> retry_count) {

    static constexpr char media_ns[] = "http://www.onvif.org/ver10/media/wsdl";
    static constexpr char media2_ns[] = "http://www.onvif.org/ver20/media/wsdl";

    auto invoker = [=](const std::string &user_name, const std::string &pwd) {
        ++*retry_count;
        asyncGetStreamUri_l(onvif_url, user_name, pwd, cb, retry_count);
    };
    SoapUtil::sendGetDeviceInformation(onvif_url, user_name, pwd, [=](const SoapObject &body, const SoapErr &err) {
        if (err) {
            cb(err, invoker, *retry_count, "");
            return;
        }

        SoapUtil::sendGetServices(onvif_url, {media_ns, media2_ns}, user_name, pwd,
        [=](const SoapErr &err, SoapUtil::onGetServicesResponseMap &mp) {
            auto media1_url = mp[media_ns];
            auto media2_url = mp[media2_ns];
            auto media_url = media2_url.empty() ? media1_url : media2_url;
            bool is_media2 = media2_url.empty() ? false : true;
            if (err) {
                cb(err, invoker, *retry_count, "");
                return;
            }
            if (media_url.empty()) {
                static constexpr char action_url[] = R"("http://www.onvif.org/ver10/device/wsdl/GetServices")";
                SoapErr err(onvif_url, action_url, SockException(), mediakit::Parser(),
                            "get media service failed");
                cb(err, invoker, *retry_count, "");
                return;
            }
            SoapUtil::sendGetProfiles(is_media2, media_url, user_name, pwd,
            [=] (const SoapErr &err, const vector<SoapUtil::onGetProfilesResponseTuple> &profile) {
                if (err) {
                    cb(err, invoker, *retry_count, "");
                    return;
                }
                if (profile.empty()) {
                    static constexpr char action_url[] = R"("http://www.onvif.org/ver10/media/wsdl/GetProfiles")";
                    static constexpr char action_url2[] = R"("http://www.onvif.org/ver20/media/wsdl/GetProfiles")";
                    SoapErr err(onvif_url, is_media2 ? action_url2 : action_url, SockException(), mediakit::Parser(),
                                "get media profile failed");
                    cb(err, invoker, *retry_count, "");
                    return;
                }
                auto profile_name = get<0>(profile[0]);
                SoapUtil::sendGetStreamUri(is_media2, media_url, profile_name, user_name, pwd,
                [=](const SoapErr &err, const string &uri) {
                    if (err) {
                        cb(err, invoker, *retry_count, "");
                        return;
                    }
                    cb(err, invoker, *retry_count, getRtspUrlWithAuth(user_name, pwd, uri));
                });
            });
        });
    });
}

void SoapUtil::asyncGetStreamUri(const std::string &onvif_url, const SoapUtil::AsyncGetStreamUriCB &cb) {
    asyncGetStreamUri_l(onvif_url, "", "", cb, std::make_shared<int>(0));
}