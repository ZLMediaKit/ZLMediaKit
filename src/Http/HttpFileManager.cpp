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
#include "Util/File.h"
#include "Common/Parser.h"
#include "Common/config.h"
#include "Common/strCoding.h"
#include "Record/HlsMediaSource.h"
#include "HttpConst.h"
#include "HttpSession.h"
#include "HttpFileManager.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

// hls的播放cookie缓存时间默认60秒，  [AUTO-TRANSLATED:88198dfa]
// The default cache time for the hls playback cookie is 60 seconds.
// 每次访问一次该cookie，那么将重新刷新cookie有效期  [AUTO-TRANSLATED:a1b76209]
// Each time this cookie is accessed, the cookie's validity period will be refreshed.
// 假如播放器在60秒内都未访问该cookie，那么将重新触发hls播放鉴权  [AUTO-TRANSLATED:55000c94]
// If the player does not access the cookie within 60 seconds, the hls playback authentication will be triggered again.
static size_t kHlsCookieSecond = 60;
static size_t kFindSrcIntervalSecond = 3;
static const string kCookieName = "ZL_COOKIE";
static const string kHlsSuffix = "/hls.m3u8";
static const string kHlsFMP4Suffix = "/hls.fmp4.m3u8";

struct HttpCookieAttachment {
    // 是否已经查找到过MediaSource  [AUTO-TRANSLATED:b5b9922a]
    // Whether the MediaSource has been found
    bool _find_src = false;
    // 查找MediaSource计时  [AUTO-TRANSLATED:39904ba9]
    // MediaSource search timing
    Ticker _find_src_ticker;
    // cookie生效作用域，本cookie只对该目录下的文件生效  [AUTO-TRANSLATED:7a59ad9a]
    // Cookie effective scope, this cookie only takes effect for files under this directory
    string _path;
    // 上次鉴权失败信息,为空则上次鉴权成功  [AUTO-TRANSLATED:de48b753]
    // Last authentication failure information, empty means last authentication succeeded
    string _err_msg;
    // hls直播时的其他一些信息，主要用于播放器个数计数以及流量计数  [AUTO-TRANSLATED:790de53a]
    // Other information during hls live broadcast, mainly used for player count and traffic count
    HlsCookieData::Ptr _hls_data;
};

const string &HttpFileManager::getContentType(const char *name) {
    return HttpConst::getHttpContentType(name);
}

namespace {
class UInt128 {
public:
    UInt128() = default;

    UInt128(const struct sockaddr_storage &storage) {
        _family = storage.ss_family;
        memset(_bytes, 0, 16);
        switch (storage.ss_family) {
            case AF_INET: {
                memcpy(_bytes, &(reinterpret_cast<const struct sockaddr_in &>(storage).sin_addr), 4);
                break;
            }
            case AF_INET6: {
                memcpy(_bytes, &(reinterpret_cast<const struct sockaddr_in6 &>(storage).sin6_addr), 16);
                break;
            }
            default: CHECK(false, "Invalid socket family"); break;
        }
    }

    bool operator==(const UInt128 &that) const { return _family == that._family && !memcmp(_bytes, that._bytes, 16); }

    bool operator<=(const UInt128 &that) const { return *this < that || *this == that; }

    bool operator>=(const UInt128 &that) const { return *this > that || *this == that; }

    bool operator>(const UInt128 &that) const { return that < *this; }

    bool operator<(const UInt128 &that) const {
        auto sz = _family == AF_INET ? 4 : 16;
        for (int i = 0; i < sz; ++i) {
            if (_bytes[i] < that._bytes[i]) {
                return true;
            } else if (_bytes[i] > that._bytes[i]) {
                return false;
            }
        }
        return false;
    }

    operator bool() const { return _family != -1; }

    bool same_type(const UInt128 &that) const { return _family == that._family; }

private:
    int _family = -1;
    uint8_t _bytes[16];
};

}

static UInt128 get_ip_uint64(const std::string &ip) {
    try {
        return UInt128(SockUtil::make_sockaddr(ip.data(), 0));
    } catch (std::exception &ex) {
        WarnL << ex.what();
    }
    return UInt128();
}

bool HttpFileManager::isIPAllowed(const std::string &ip) {
    using IPRangs = std::vector<std::pair<UInt128 /*min_ip*/, UInt128 /*max_ip*/>>;
    GET_CONFIG_FUNC(IPRangs, allow_ip_range, Http::kAllowIPRange, [](const string &str) -> IPRangs {
        IPRangs ret;
        auto vec = split(str, ",");
        for (auto &item : vec) {
            if (trim(item).empty()) {
                continue;
            }
            auto range = split(item, "-");
            if (range.size() == 2) {
                auto ip_min = get_ip_uint64(trim(range[0]));
                auto ip_max = get_ip_uint64(trim(range[1]));
                if (ip_min && ip_max && ip_min.same_type(ip_max)) {
                    ret.emplace_back(ip_min, ip_max);
                } else {
                    WarnL << "Invalid ip range or family: " << item;
                }
            } else if (range.size() == 1) {
                auto ip = get_ip_uint64(trim(range[0]));
                if (ip) {
                    ret.emplace_back(ip, ip);
                } else {
                    WarnL << "Invalid ip: " << item;
                }
            } else {
                WarnL << "Invalid ip range: " << item;
            }
        }
        return ret;
    });

    if (allow_ip_range.empty()) {
        return true;
    }
    auto ip_int = get_ip_uint64(ip);
    for (auto &range : allow_ip_range) {
        if (ip_int.same_type(range.first) && ip_int >= range.first && ip_int <= range.second) {
            return true;
        }
    }
    return false;
}

static std::string fileName(const string &dir, const string &path) {
    auto ret = path.substr(dir.size());
    if (ret.front() == '/') {
        ret.erase(0, 1);
    }
    return ret;
}

static string searchIndexFile(const string &dir) {
    std::string ret;
    static set<std::string, StrCaseCompare> indexSet = { "index.html", "index.htm" };
    File::scanDir(dir, [&](const string &path, bool is_dir) {
        if (is_dir) {
            return true;
        }
        auto name = fileName(dir, path);
        if (indexSet.find(name) == indexSet.end()) {
            return true;
        }
        ret = std::move(name);
        return false;
    });
    return ret;
}

static bool makeFolderMenu(const string &httpPath, const string &strFullPath, string &strRet) {
    GET_CONFIG(bool, dirMenu, Http::kDirMenu);
    if (!dirMenu) {
        // 不允许浏览文件夹  [AUTO-TRANSLATED:a0c30a94]
        // Not allowed to browse folders
        return false;
    }
    string strPathPrefix(strFullPath);
    // url后缀有没有'/'访问文件夹，处理逻辑不一致  [AUTO-TRANSLATED:39c6a933]
    // Whether the url suffix has '/' to access the folder, the processing logic is inconsistent
    string last_dir_name;
    if (strPathPrefix.back() == '/') {
        strPathPrefix.pop_back();
    } else {
        last_dir_name = split(strPathPrefix, "/").back();
    }

    if (!File::is_dir(strPathPrefix)) {
        return false;
    }
    stringstream ss;
    ss << "<html>\r\n"
          "<head>\r\n"
          "<title>File Index</title>\r\n"
          "</head>\r\n"
          "<body>\r\n"
          "<h1>Index of ";

    ss << httpPath;
    ss << "</h1>\r\n";
    if (httpPath != "/") {
        ss << "<li><a href=\"";
        ss << "/";
        ss << "\">";
        ss << "root";
        ss << "</a></li>\r\n";

        ss << "<li><a href=\"";
        if (!last_dir_name.empty()) {
            ss << "./";
        } else {
            ss << "../";
        }
        ss << "\">";
        ss << "../";
        ss << "</a></li>\r\n";
    }

    multimap<string/*url name*/, std::pair<string/*note name*/, string/*file path*/> > file_map;
    File::scanDir(strPathPrefix, [&](const std::string &path, bool isDir) {
        auto name = fileName(strPathPrefix, path);
        file_map.emplace(strCoding::UrlEncodePath(name), std::make_pair(name, path));
        return true;
    });
    // 如果是root目录，添加虚拟目录  [AUTO-TRANSLATED:3149d7f9]
    // If it is the root directory, add a virtual directory
    if (httpPath == "/") {
        GET_CONFIG_FUNC(StrCaseMap, virtualPathMap, Http::kVirtualPath, [](const string &str) {
            return Parser::parseArgs(str, ";", ",");
        });
        for (auto &pr : virtualPathMap) {
            file_map.emplace(pr.first, std::make_pair(string("virtual path: ") + pr.first, File::absolutePath("", pr.second)));
        }
    }
    int i = 0;
    for (auto &pr :file_map) {
        auto &strAbsolutePath = pr.second.second;
        bool isDir = File::is_dir(strAbsolutePath);
        ss << "<li><span>" << i++ << "</span>\t";
        ss << "<a href=\"";
        // 路径链接地址  [AUTO-TRANSLATED:33bc5f41]
        // Path link address
        if (!last_dir_name.empty()) {
            ss << last_dir_name << "/" << pr.first;
        } else {
            ss << pr.first;
        }

        if (isDir) {
            ss << "/";
        }
        ss << "\">";
        // 路径名称  [AUTO-TRANSLATED:4dae8790]
        // Path name
        ss << pr.second.first;
        if (isDir) {
            ss << "/</a></li>\r\n";
            continue;
        }
        // 是文件  [AUTO-TRANSLATED:70473f2f]
        // It's a file
        auto fileSize = File::fileSize(strAbsolutePath);
        if (fileSize < 1024) {
            ss << " (" << fileSize << "B)" << endl;
        } else if (fileSize < 1024 * 1024) {
            ss << fixed << setprecision(2) << " (" << fileSize / 1024.0 << "KB)";
        } else if (fileSize < 1024 * 1024 * 1024) {
            ss << fixed << setprecision(2) << " (" << fileSize / 1024 / 1024.0 << "MB)";
        } else {
            ss << fixed << setprecision(2) << " (" << fileSize / 1024 / 1024 / 1024.0 << "GB)";
        }
        ss << "</a></li>\r\n";
    }
    ss << "<ul>\r\n";
    ss << "</ul>\r\n</body></html>";
    ss.str().swap(strRet);
    return true;
}

// 拦截hls的播放请求  [AUTO-TRANSLATED:dd1bbeec]
// Intercept the hls playback request
static bool emitHlsPlayed(const Parser &parser, const MediaInfo &media_info, const HttpSession::HttpAccessPathInvoker &invoker,Session &sender){
    // 访问的hls.m3u8结尾，我们转换成kBroadcastMediaPlayed事件  [AUTO-TRANSLATED:b7a67c84]
    // The hls.m3u8 ending of the access, we convert it to the kBroadcastMediaPlayed event
    Broadcast::AuthInvoker auth_invoker = [invoker](const string &err) {
        // cookie有效期为kHlsCookieSecond  [AUTO-TRANSLATED:a0026dcd]
        // The cookie validity period is kHlsCookieSecond
        invoker(err, "", kHlsCookieSecond);
    };
    bool flag = NOTICE_EMIT(BroadcastMediaPlayedArgs, Broadcast::kBroadcastMediaPlayed, media_info, auth_invoker, sender);
    if (!flag) {
        // 未开启鉴权，那么允许播放  [AUTO-TRANSLATED:077feed1]
        // Authentication is not enabled, so playback is allowed
        auth_invoker("");
    }
    return flag;
}

class SockInfoImp : public SockInfo{
public:
    using Ptr = std::shared_ptr<SockInfoImp>;

    string get_local_ip() override {
        return _local_ip;
    }

    uint16_t get_local_port() override {
        return _local_port;
    }

    string get_peer_ip() override {
        return _peer_ip;
    }

    uint16_t get_peer_port() override {
        return _peer_port;
    }

    string getIdentifier() const override {
        return _identifier;
    }

    string _local_ip;
    string _peer_ip;
    string _identifier;
    uint16_t _local_port;
    uint16_t _peer_port;
};

/**
 * 判断http客户端是否有权限访问文件的逻辑步骤
 * 1、根据http请求头查找cookie，找到进入步骤3
 * 2、根据http url参数查找cookie，如果还是未找到cookie则进入步骤5
 * 3、cookie标记是否有权限访问文件，如果有权限，直接返回文件
 * 4、cookie中记录的url参数是否跟本次url参数一致，如果一致直接返回客户端错误码
 * 5、触发kBroadcastHttpAccess事件
 * The logical steps to determine whether the http client has permission to access the file
 * 1. Find the cookie according to the http request header, find it and enter step 3
 * 2. Find the cookie according to the http url parameter, if the cookie is still not found, enter step 5
 * 3. Whether the cookie mark has permission to access the file, if it has permission, return the file directly
 * 4. Whether the url parameter recorded in the cookie is consistent with the current url parameter, if it is consistent, return the client error code directly
 * 5. Trigger the kBroadcastHttpAccess event
 
 * [AUTO-TRANSLATED:dfc0f15f]
 */
static void canAccessPath(Session &sender, const Parser &parser, const MediaInfo &media_info, bool is_dir,
                          const function<void(const string &err_msg, const HttpServerCookie::Ptr &cookie)> &callback) {
    // 获取用户唯一id  [AUTO-TRANSLATED:5b1cf4bf]
    // Get the user's unique id
    auto uid = parser.params();
    auto path = parser.url();

    // 先根据http头中的cookie字段获取cookie  [AUTO-TRANSLATED:155cf682]
    // First get the cookie according to the cookie field in the http header
    HttpServerCookie::Ptr cookie = HttpCookieManager::Instance().getCookie(kCookieName, parser.getHeader());
    // 是否需要更新cookie  [AUTO-TRANSLATED:b95121d5]
    // Whether to update the cookie
    bool update_cookie = false;
    if (!cookie && !uid.empty()) {
        // 客户端请求中无cookie,再根据该用户的用户id获取cookie  [AUTO-TRANSLATED:42cb8ade]
        // There is no cookie in the client request, then get the cookie according to the user id of the user
        cookie = HttpCookieManager::Instance().getCookieByUid(kCookieName, uid);
        update_cookie = true;
    }

    if (cookie) {
        auto& attach = cookie->getAttach<HttpCookieAttachment>();
        if (path.find(attach._path) == 0) {
            // 上次cookie是限定本目录  [AUTO-TRANSLATED:a5c40abf]
            // The last cookie is limited to this directory
            if (attach._err_msg.empty()) {
                // 上次鉴权成功  [AUTO-TRANSLATED:1a23f781]
                // Last authentication succeeded
                if (attach._hls_data) {
                    // 如果播放的是hls，那么刷新hls的cookie(获取ts文件也会刷新)  [AUTO-TRANSLATED:02acac59]
                    // If the playback is hls, then refresh the hls cookie (getting the ts file will also refresh)
                    cookie->updateTime();
                    update_cookie = true;
                }
                callback("", update_cookie ? cookie : nullptr);
                return;
            }
            // 上次鉴权失败，但是如果url参数发生变更，那么也重新鉴权下  [AUTO-TRANSLATED:df9bd345]
            // Last authentication failed, but if the url parameter changes, then re-authenticate
            if (parser.params().empty() || parser.params() == cookie->getUid()) {
                // url参数未变，或者本来就没有url参数，那么判断本次请求为重复请求，无访问权限  [AUTO-TRANSLATED:f46b4fca]
                // The url parameter has not changed, or there is no url parameter at all, then determine that the current request is a duplicate request and has no access permission
                callback(attach._err_msg, update_cookie ? cookie : nullptr);
                return;
            }
        }
        // 如果url参数变了或者不是限定本目录，那么旧cookie失效，重新鉴权  [AUTO-TRANSLATED:acf6d49e]
        // If the url parameter changes or is not limited to this directory, then the old cookie expires and re-authentication is required
        HttpCookieManager::Instance().delCookie(cookie);
    }

    bool is_hls = media_info.schema == HLS_SCHEMA || media_info.schema == HLS_FMP4_SCHEMA;

    SockInfoImp::Ptr info = std::make_shared<SockInfoImp>();
    info->_identifier = sender.getIdentifier();
    info->_peer_ip = sender.get_peer_ip();
    info->_peer_port = sender.get_peer_port();
    info->_local_ip = sender.get_local_ip();
    info->_local_port = sender.get_local_port();

    // 该用户从来未获取过cookie，这个时候我们广播是否允许该用户访问该http目录  [AUTO-TRANSLATED:8f4b3dd2]
    // This user has never obtained a cookie, at this time we broadcast whether to allow this user to access this http directory
    HttpSession::HttpAccessPathInvoker accessPathInvoker = [callback, uid, path, is_dir, is_hls, media_info, info]
            (const string &err_msg, const string &cookie_path_in, int life_second) {
        HttpServerCookie::Ptr cookie;
        if (life_second) {
            // 本次鉴权设置了有效期，我们把鉴权结果缓存在cookie中  [AUTO-TRANSLATED:5a12f48e]
            // This authentication has an expiration date, we cache the authentication result in the cookie
            string cookie_path = cookie_path_in;
            if (cookie_path.empty()) {
                // 如果未设置鉴权目录，那么我们采用当前目录  [AUTO-TRANSLATED:701ada2d]
                // If no authentication directory is set, we use the current directory
                cookie_path = is_dir ? path : path.substr(0, path.rfind("/") + 1);
            }

            auto attach = std::make_shared<HttpCookieAttachment>();
            // 记录用户能访问的路径  [AUTO-TRANSLATED:80a2ba33]
            // Record the paths that the user can access
            attach->_path = cookie_path;
            // 记录能否访问  [AUTO-TRANSLATED:972f6fc5]
            // Record whether access is allowed
            attach->_err_msg = err_msg;
            if (is_hls) {
                // hls相关信息  [AUTO-TRANSLATED:37893a71]
                // hls related information
                attach->_hls_data = std::make_shared<HlsCookieData>(media_info, info);
            }
           toolkit::Any any;
           any.set(std::move(attach));
           callback(err_msg, HttpCookieManager::Instance().addCookie(kCookieName, uid, life_second, std::move(any)));
        } else {
            callback(err_msg, nullptr);
        }
    };

    if (is_hls) {
        // 是hls的播放鉴权,拦截之  [AUTO-TRANSLATED:c5ba86bb]
        // This is hls playback authentication, intercept it
        emitHlsPlayed(parser, media_info, accessPathInvoker, sender);
        return;
    }

    // 事件未被拦截，则认为是http下载请求  [AUTO-TRANSLATED:7d449ccc]
    // The event was not intercepted, it is considered an http download request
    bool flag = NOTICE_EMIT(BroadcastHttpAccessArgs, Broadcast::kBroadcastHttpAccess, parser, path, is_dir, accessPathInvoker, sender);
    if (!flag) {
        // 此事件无人监听，我们默认都有权限访问  [AUTO-TRANSLATED:e1524c0f]
        // No one is listening to this event, we assume that everyone has permission to access it by default
        callback("", nullptr);
    }
}

/**
 * 发送404 Not Found
 * Send 404 Not Found
 
 * [AUTO-TRANSLATED:1297f2e7]
 */
static void sendNotFound(const HttpFileManager::invoker &cb) {
    GET_CONFIG(string, notFound, Http::kNotFound);
    cb(404, "text/html", StrCaseMap(), std::make_shared<HttpStringBody>(notFound));
}

/**
 * 拼接文件路径
 * Concatenate the file path
 
 * [AUTO-TRANSLATED:cf6f5c53]
 */
static string pathCat(const string &a, const string &b){
    if (a.back() == '/') {
        return a + b;
    }
    return a + '/' + b;
}

/**
 * 访问文件
 * @param sender 事件触发者
 * @param parser http请求
 * @param media_info http url信息
 * @param file_path 文件绝对路径
 * @param cb 回调对象
 * Access the file
 * @param sender Event trigger
 * @param parser http request
 * @param media_info http url information
 * @param file_path Absolute file path
 * @param cb Callback object
 
 * [AUTO-TRANSLATED:2d840fe6]
 */
static void accessFile(Session &sender, const Parser &parser, const MediaInfo &media_info, const string &file_path, const HttpFileManager::invoker &cb) {
    bool is_hls = end_with(file_path, kHlsSuffix) || end_with(file_path, kHlsFMP4Suffix);
    if (!is_hls && !File::fileExist(file_path)) {
        // 文件不存在且不是hls,那么直接返回404  [AUTO-TRANSLATED:7aae578b]
        // The file does not exist and is not hls, so directly return 404
        sendNotFound(cb);
        return;
    }
    if (is_hls) {
        // hls，那么移除掉后缀获取真实的stream_id并且修改协议为HLS  [AUTO-TRANSLATED:94b5818a]
        // hls, then remove the suffix to get the real stream_id and change the protocol to HLS
        if (end_with(file_path, kHlsSuffix)) {
            const_cast<string &>(media_info.schema) = HLS_SCHEMA;
            replace(const_cast<string &>(media_info.stream), kHlsSuffix, "");
        } else {
            const_cast<string &>(media_info.schema) = HLS_FMP4_SCHEMA;
            replace(const_cast<string &>(media_info.stream), kHlsFMP4Suffix, "");
        }
    }

    weak_ptr<Session> weakSession = static_pointer_cast<Session>(sender.shared_from_this());
    // 判断是否有权限访问该文件  [AUTO-TRANSLATED:b7f595f5]
    // Determine whether you have permission to access this file
    canAccessPath(sender, parser, media_info, false, [cb, file_path, parser, is_hls, media_info, weakSession](const string &err_msg, const HttpServerCookie::Ptr &cookie) {
        auto strongSession = weakSession.lock();
        if (!strongSession) {
            // http客户端已经断开，不需要回复  [AUTO-TRANSLATED:9a252e21]
            // The http client has disconnected and does not need to reply
            return;
        }
        if (!err_msg.empty()) {
            // 文件鉴权失败  [AUTO-TRANSLATED:0feb8885]
            // File authentication failed
            StrCaseMap headerOut;
            if (cookie) {
                headerOut["Set-Cookie"] = cookie->getCookie(cookie->getAttach<HttpCookieAttachment>()._path);
            }
            cb(401, "text/html", headerOut, std::make_shared<HttpStringBody>(err_msg));
            return;
        }

        auto response_file = [is_hls](const HttpServerCookie::Ptr &cookie, const HttpFileManager::invoker &cb, const string &file_path, const Parser &parser, const string &file_content = "") {
            StrCaseMap httpHeader;
            if (cookie) {
                httpHeader["Set-Cookie"] = cookie->getCookie(cookie->getAttach<HttpCookieAttachment>()._path);
            }
            HttpSession::HttpResponseInvoker invoker = [&](int code, const StrCaseMap &headerOut, const HttpBody::Ptr &body) {
                if (cookie && body) {
                    auto& attach = cookie->getAttach<HttpCookieAttachment>();
                    if (attach._hls_data) {
                        attach._hls_data->addByteUsage(body->remainSize());
                    }
                }
                cb(code, HttpFileManager::getContentType(file_path.data()), headerOut, body);
            };
            GET_CONFIG_FUNC(vector<string>, forbidCacheSuffix, Http::kForbidCacheSuffix, [](const string &str) {
                return split(str, ",");
            });
            bool is_forbid_cache = false;
            for (auto &suffix : forbidCacheSuffix) {
                if (suffix != "" && end_with(file_path, suffix)) {
                    is_forbid_cache = true;
                    break;
                }
            }
            invoker.responseFile(parser.getHeader(), httpHeader, file_content.empty() ? file_path : file_content, !is_hls && !is_forbid_cache, file_content.empty());
        };

        if (!is_hls || !cookie) {
            // 不是hls或访问m3u8文件不带cookie, 直接回复文件或404  [AUTO-TRANSLATED:64e5d19b]
            // Not hls or accessing m3u8 files without cookies, directly reply to the file or 404
            response_file(cookie, cb, file_path, parser);
            if (is_hls) {
                WarnL << "access m3u8 file without cookie:" << file_path;
            }
            return;
        }

        auto &attach = cookie->getAttach<HttpCookieAttachment>();
        auto src = attach._hls_data->getMediaSource();
        if (src) {
            // 直接从内存获取m3u8索引文件(而不是从文件系统)  [AUTO-TRANSLATED:c772e342]
            // Get the m3u8 index file directly from memory (instead of from the file system)
            response_file(cookie, cb, file_path, parser, src->getIndexFile());
            return;
        }
        if (attach._find_src && attach._find_src_ticker.elapsedTime() < kFindSrcIntervalSecond * 1000) {
            // 最近已经查找过MediaSource了，为了防止频繁查找导致占用全局互斥锁的问题，我们尝试直接从磁盘返回hls索引文件  [AUTO-TRANSLATED:a33d5e4d]
            // MediaSource has been searched recently, in order to prevent frequent searches from occupying the global mutex, we try to return the hls index file directly from the disk
            response_file(cookie, cb, file_path, parser);
            return;
        }

        // hls流可能未注册，MediaSource::findAsync可以触发not_found事件，然后再按需推拉流  [AUTO-TRANSLATED:f4acd717]
        // The hls stream may not be registered, MediaSource::findAsync can trigger the not_found event, and then push and pull the stream on demand
        MediaSource::findAsync(media_info, strongSession, [response_file, cookie, cb, file_path, parser](const MediaSource::Ptr &src) {
            auto hls = dynamic_pointer_cast<HlsMediaSource>(src);
            if (!hls) {
                // 流不在线  [AUTO-TRANSLATED:5a6a5695]
                // The stream is not online
                response_file(cookie, cb, file_path, parser);
                return;
            }

            auto &attach = cookie->getAttach<HttpCookieAttachment>();
            attach._hls_data->setMediaSource(hls);
            // 添加HlsMediaSource的观看人数(HLS是按需生成的，这样可以触发HLS文件的生成)  [AUTO-TRANSLATED:bd98e100]
            // Add the number of viewers of HlsMediaSource (HLS is generated on demand, so this can trigger the generation of HLS files)
            attach._hls_data->addByteUsage(0);
            // 标记找到MediaSource  [AUTO-TRANSLATED:1e298005]
            // Mark that MediaSource has been found
            attach._find_src = true;

            // 重置查找MediaSource计时  [AUTO-TRANSLATED:d1e47e07]
            // Reset the MediaSource search timer
            attach._find_src_ticker.resetTime();

            // m3u8文件可能不存在, 等待m3u8索引文件按需生成  [AUTO-TRANSLATED:0dbd4df2]
            // The m3u8 file may not exist, wait for the m3u8 index file to be generated on demand
            hls->getIndexFile([response_file, file_path, cookie, cb, parser](const string &file) {
                response_file(cookie, cb, file_path, parser, file);
            });
        });
    });
}

static string getFilePath(const Parser &parser,const MediaInfo &media_info, Session &sender) {
    GET_CONFIG(bool, enableVhost, General::kEnableVhost);
    GET_CONFIG(string, rootPath, Http::kRootPath);
    GET_CONFIG_FUNC(StrCaseMap, virtualPathMap, Http::kVirtualPath, [](const string &str) {
        return Parser::parseArgs(str, ";", ",");
    });

    string url, path, virtual_app;
    auto it = virtualPathMap.find(media_info.app);
    if (it != virtualPathMap.end()) {
        // 访问的是virtualPath  [AUTO-TRANSLATED:a36c7b20]
        // Accessing virtualPath
        path = it->second;
        url = parser.url().substr(1 + media_info.app.size());
        virtual_app = media_info.app + "/";
    } else {
        // 访问的是rootPath  [AUTO-TRANSLATED:600765f0]
        // Accessing rootPath
        path = rootPath;
        url = parser.url();
    }
    for (auto &ch : url) {
        if (ch == '\\') {
            // 如果url中存在"\"，这种目录是Windows样式的；需要批量转换为标准的"/"; 防止访问目录权限外的文件  [AUTO-TRANSLATED:fd6b5900]
            // If the url contains "\", this directory is in Windows style; it needs to be converted to standard "/" in batches; prevent access to files outside the directory permissions
            ch = '/';
        }
    }
    auto ret = File::absolutePath(enableVhost ? media_info.vhost + url : url, path);
    auto http_root = File::absolutePath(enableVhost ? media_info.vhost + "/" : "/", path);
    if (!start_with(ret, http_root)) {
        // 访问的http文件不得在http根目录之外  [AUTO-TRANSLATED:7d85a8f9]
        // The accessed http file must not be outside the http root directory
        throw std::runtime_error("Attempting to access files outside of the http root directory");
    }
    // 替换url，防止返回的目录索引网页被注入非法内容  [AUTO-TRANSLATED:463ad1b1]
    // Replace the url to prevent the returned directory index page from being injected with illegal content
    const_cast<Parser&>(parser).setUrl("/" + virtual_app + ret.substr(http_root.size()));
    NOTICE_EMIT(BroadcastHttpBeforeAccessArgs, Broadcast::kBroadcastHttpBeforeAccess, parser, ret, sender);
    return ret;
}

/**
 * 访问文件或文件夹
 * @param sender 事件触发者
 * @param parser http请求
 * @param cb 回调对象
 * Access file or folder
 * @param sender Event trigger
 * @param parser http request
 * @param cb Callback object
 
 * [AUTO-TRANSLATED:a79c824d]
 */
void HttpFileManager::onAccessPath(Session &sender, Parser &parser, const HttpFileManager::invoker &cb) {
    auto fullUrl = "http://" + parser["Host"] + parser.fullUrl();
    MediaInfo media_info(fullUrl);
    auto file_path = getFilePath(parser, media_info, sender);
    if (file_path.size() == 0) {
        sendNotFound(cb);
        return;
    }
    // 访问的是文件夹  [AUTO-TRANSLATED:279974bb]
    // Accessing a folder
    if (File::is_dir(file_path)) {
        auto indexFile = searchIndexFile(file_path);
        if (!indexFile.empty()) {
            // 发现该文件夹下有index文件  [AUTO-TRANSLATED:4a697758]
            // Found index file in this folder
            file_path = pathCat(file_path, indexFile);
            if (!File::is_dir(file_path)) {
                // 不是文件夹  [AUTO-TRANSLATED:af893469]
                // Not a folder
                parser.setUrl(pathCat(parser.url(), indexFile));
                accessFile(sender, parser, media_info, file_path, cb);
                return;
            }
        }
        string strMenu;
        // 生成文件夹菜单索引  [AUTO-TRANSLATED:04150cc8]
        // Generate folder menu index
        if (!makeFolderMenu(parser.url(), file_path, strMenu)) {
            // 文件夹不存在  [AUTO-TRANSLATED:a2dc6c89]
            // Folder does not exist
            sendNotFound(cb);
            return;
        }
        // 判断是否有权限访问该目录  [AUTO-TRANSLATED:963d02a6]
        // Determine if there is permission to access this directory
        canAccessPath(sender, parser, media_info, true, [strMenu, cb](const string &err_msg, const HttpServerCookie::Ptr &cookie) mutable{
            if (!err_msg.empty()) {
                strMenu = err_msg;
            }
            StrCaseMap headerOut;
            if (cookie) {
                headerOut["Set-Cookie"] = cookie->getCookie(cookie->getAttach<HttpCookieAttachment>()._path);
            }
            cb(err_msg.empty() ? 200 : 401, "text/html", headerOut, std::make_shared<HttpStringBody>(strMenu));
        });
        return;
    }

    // 访问的是文件  [AUTO-TRANSLATED:7a400b3c]
    // Accessing a file
    accessFile(sender, parser, media_info, file_path, cb);
};


////////////////////////////////////HttpResponseInvokerImp//////////////////////////////////////

void HttpResponseInvokerImp::operator()(int code, const StrCaseMap &headerOut, const Buffer::Ptr &body) const {
    return operator()(code, headerOut, std::make_shared<HttpBufferBody>(body));
}

void HttpResponseInvokerImp::operator()(int code, const StrCaseMap &headerOut, const HttpBody::Ptr &body) const{
    if (_lambad) {
        _lambad(code, headerOut, body);
    }
}

void HttpResponseInvokerImp::operator()(int code, const StrCaseMap &headerOut, const string &body) const{
    this->operator()(code, headerOut, std::make_shared<HttpStringBody>(body));
}

HttpResponseInvokerImp::HttpResponseInvokerImp(const HttpResponseInvokerImp::HttpResponseInvokerLambda0 &lambda){
    _lambad = lambda;
}

HttpResponseInvokerImp::HttpResponseInvokerImp(const HttpResponseInvokerImp::HttpResponseInvokerLambda1 &lambda){
    if (!lambda) {
        _lambad = nullptr;
        return;
    }
    _lambad = [lambda](int code, const StrCaseMap &headerOut, const HttpBody::Ptr &body) {
        string str;
        if (body && body->remainSize()) {
            str = body->readData(body->remainSize())->toString();
        }
        lambda(code, headerOut, str);
    };
}

void HttpResponseInvokerImp::responseFile(const StrCaseMap &requestHeader,
                                          const StrCaseMap &responseHeader,
                                          const string &file,
                                          bool use_mmap,
                                          bool is_path) const {
    if (!is_path) {
        // file是文件内容  [AUTO-TRANSLATED:61d0be82]
        // file is the file content
        (*this)(200, responseHeader, std::make_shared<HttpStringBody>(file));
        return;
    }

    // file是文件路径  [AUTO-TRANSLATED:28dcac38]
    // file is the file path
    GET_CONFIG(string, charSet, Http::kCharSet);
    StrCaseMap &httpHeader = const_cast<StrCaseMap &>(responseHeader);
    auto fileBody = std::make_shared<HttpFileBody>(file, use_mmap);
    if (fileBody->remainSize() < 0) {
        // 打开文件失败  [AUTO-TRANSLATED:1f0405cb]
        // Failed to open file
        GET_CONFIG(string, notFound, Http::kNotFound);

        auto strContentType = StrPrinter << "text/html; charset=" << charSet << endl;
        httpHeader["Content-Type"] = strContentType;
        (*this)(404, httpHeader, notFound);
        return;
    }

    // 尝试添加Content-Type  [AUTO-TRANSLATED:2c08b371]
    // Try to add Content-Type
    httpHeader.emplace("Content-Type", HttpConst::getHttpContentType(file.data()) + "; charset=" + charSet);

    auto &strRange = const_cast<StrCaseMap &>(requestHeader)["Range"];
    int code = 200;
    if (!strRange.empty()) {
        // 分节下载  [AUTO-TRANSLATED:01920230]
        // Segmented download
        code = 206;
        auto iRangeStart = atoll(findSubString(strRange.data(), "bytes=", "-").data());
        auto iRangeEnd = atoll(findSubString(strRange.data(), "-", nullptr).data());
        auto fileSize = fileBody->remainSize();
        if (iRangeEnd == 0) {
            iRangeEnd = fileSize - 1;
        }
        // 设置文件范围  [AUTO-TRANSLATED:aa51fd28]
        // Set file range
        fileBody->setRange(iRangeStart, iRangeEnd - iRangeStart + 1);
        // 分节下载返回Content-Range头  [AUTO-TRANSLATED:4b78e7b6]
        // Segmented download returns Content-Range header
        httpHeader.emplace("Content-Range", StrPrinter << "bytes " << iRangeStart << "-" << iRangeEnd << "/" << fileSize << endl);
    }

    // 回复文件  [AUTO-TRANSLATED:5d91a916]
    // Reply file
    (*this)(code, httpHeader, fileBody);
}

HttpResponseInvokerImp::operator bool(){
    return _lambad.operator bool();
}


}//namespace mediakit
