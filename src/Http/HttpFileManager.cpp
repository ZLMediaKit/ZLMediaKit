/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <sys/stat.h>
#if !defined(_WIN32)
#include <dirent.h>
#endif //!defined(_WIN32)
#include <iomanip>
#include "HttpFileManager.h"
#include "Util/File.h"
#include "HttpSession.h"

namespace mediakit {

// hls的播放cookie缓存时间默认60秒，
// 每次访问一次该cookie，那么将重新刷新cookie有效期
// 假如播放器在60秒内都未访问该cookie，那么将重新触发hls播放鉴权
static int kHlsCookieSecond = 60;
static const string kCookieName = "ZL_COOKIE";
static const string kCookiePathKey = "kCookiePathKey";
static const string kAccessErrKey = "kAccessErrKey";
static const string kAccessHls = "kAccessHls";
static const string kHlsSuffix = "/hls.m3u8";

static const string &getContentType(const char *name) {
    const char *dot;
    dot = strrchr(name, '.');
    static StrCaseMap mapType;
    static onceToken token([&]() {
        mapType.emplace(".html", "text/html");
        mapType.emplace(".htm", "text/html");
        mapType.emplace(".mp4", "video/mp4");
        mapType.emplace(".mkv", "video/x-matroska");
        mapType.emplace(".rmvb", "application/vnd.rn-realmedia");
        mapType.emplace(".rm", "application/vnd.rn-realmedia");
        mapType.emplace(".m3u8", "application/vnd.apple.mpegurl");
        mapType.emplace(".jpg", "image/jpeg");
        mapType.emplace(".jpeg", "image/jpeg");
        mapType.emplace(".gif", "image/gif");
        mapType.emplace(".png", "image/png");
        mapType.emplace(".ico", "image/x-icon");
        mapType.emplace(".css", "text/css");
        mapType.emplace(".js", "application/javascript");
        mapType.emplace(".au", "audio/basic");
        mapType.emplace(".wav", "audio/wav");
        mapType.emplace(".avi", "video/x-msvideo");
        mapType.emplace(".mov", "video/quicktime");
        mapType.emplace(".qt", "video/quicktime");
        mapType.emplace(".mpeg", "video/mpeg");
        mapType.emplace(".mpe", "video/mpeg");
        mapType.emplace(".vrml", "model/vrml");
        mapType.emplace(".wrl", "model/vrml");
        mapType.emplace(".midi", "audio/midi");
        mapType.emplace(".mid", "audio/midi");
        mapType.emplace(".mp3", "audio/mpeg");
        mapType.emplace(".ogg", "application/ogg");
        mapType.emplace(".pac", "application/x-ns-proxy-autoconfig");
        mapType.emplace(".flv", "video/x-flv");
    });
    static string defaultType = "text/plain";
    if (!dot) {
        return defaultType;
    }
    auto it = mapType.find(dot);
    if (it == mapType.end()) {
        return defaultType;
    }
    return it->second;
}

static string searchIndexFile(const string &dir){
    DIR *pDir;
    dirent *pDirent;
    if ((pDir = opendir(dir.data())) == NULL) {
        return "";
    }
    set<string> setFile;
    while ((pDirent = readdir(pDir)) != NULL) {
        static set<const char *,StrCaseCompare> indexSet = {"index.html","index.htm","index"};
        if(indexSet.find(pDirent->d_name) !=  indexSet.end()){
            string ret = pDirent->d_name;
            closedir(pDir);
            return ret;
        }
    }
    closedir(pDir);
    return "";
}

static bool makeFolderMenu(const string &httpPath, const string &strFullPath, string &strRet) {
    string strPathPrefix(strFullPath);
    string last_dir_name;
    if(strPathPrefix.back() == '/'){
        strPathPrefix.pop_back();
    }else{
        last_dir_name = split(strPathPrefix,"/").back();
    }

    if (!File::is_dir(strPathPrefix.data())) {
        return false;
    }
    stringstream ss;
    ss <<   "<html>\r\n"
            "<head>\r\n"
            "<title>文件索引</title>\r\n"
            "</head>\r\n"
            "<body>\r\n"
            "<h1>文件索引:";

    ss << httpPath;
    ss << "</h1>\r\n";
    if (httpPath != "/") {
        ss << "<li><a href=\"";
        ss << "/";
        ss << "\">";
        ss << "根目录";
        ss << "</a></li>\r\n";

        ss << "<li><a href=\"";
        if(!last_dir_name.empty()){
            ss << "./";
        }else{
            ss << "../";
        }
        ss << "\">";
        ss << "上级目录";
        ss << "</a></li>\r\n";
    }

    DIR *pDir;
    dirent *pDirent;
    if ((pDir = opendir(strPathPrefix.data())) == NULL) {
        return false;
    }
    set<string> setFile;
    while ((pDirent = readdir(pDir)) != NULL) {
        if (File::is_special_dir(pDirent->d_name)) {
            continue;
        }
        if(pDirent->d_name[0] == '.'){
            continue;
        }
        setFile.emplace(pDirent->d_name);
    }
    int i = 0;
    for(auto &strFile :setFile ){
        string strAbsolutePath = strPathPrefix + "/" + strFile;
        bool isDir = File::is_dir(strAbsolutePath.data());
        ss << "<li><span>" << i++ << "</span>\t";
        ss << "<a href=\"";
        if(!last_dir_name.empty()){
            ss << last_dir_name << "/" << strFile;
        }else{
            ss << strFile;
        }

        if(isDir){
            ss << "/";
        }
        ss << "\">";
        ss << strFile;
        if (isDir) {
            ss << "/</a></li>\r\n";
            continue;
        }
        //是文件
        struct stat fileData;
        if (0 == stat(strAbsolutePath.data(), &fileData)) {
            auto &fileSize = fileData.st_size;
            if (fileSize < 1024) {
                ss << " (" << fileData.st_size << "B)" << endl;
            } else if (fileSize < 1024 * 1024) {
                ss << fixed << setprecision(2) << " (" << fileData.st_size / 1024.0 << "KB)";
            } else if (fileSize < 1024 * 1024 * 1024) {
                ss << fixed << setprecision(2) << " (" << fileData.st_size / 1024 / 1024.0 << "MB)";
            } else {
                ss << fixed << setprecision(2) << " (" << fileData.st_size / 1024 / 1024 / 1024.0 << "GB)";
            }
        }
        ss << "</a></li>\r\n";
    }
    closedir(pDir);
    ss << "<ul>\r\n";
    ss << "</ul>\r\n</body></html>";
    ss.str().swap(strRet);
    return true;
}

//字符串是否以xx结尾
static bool end_of(const string &str, const string &substr){
    auto pos = str.rfind(substr);
    return pos != string::npos && pos == str.size() - substr.size();
};

//拦截hls的播放请求
static bool emitHlsPlayed(BroadcastHttpAccessArgs){
    //访问的hls.m3u8结尾，我们转换成kBroadcastMediaPlayed事件
    Broadcast::AuthInvoker mediaAuthInvoker = [invoker,path](const string &err){
        //cookie有效期为kHlsCookieSecond
        invoker(err,"",kHlsCookieSecond);
    };

    auto args_copy = args;
    replace(args_copy._streamid,kHlsSuffix,"");
    return NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaPlayed,args_copy,mediaAuthInvoker,sender);
}


/**
 * 判断http客户端是否有权限访问文件的逻辑步骤
 * 1、根据http请求头查找cookie，找到进入步骤3
 * 2、根据http url参数查找cookie，如果还是未找到cookie则进入步骤5
 * 3、cookie标记是否有权限访问文件，如果有权限，直接返回文件
 * 4、cookie中记录的url参数是否跟本次url参数一致，如果一致直接返回客户端错误码
 * 5、触发kBroadcastHttpAccess事件
 */
static void canAccessPath(TcpSession &sender, const Parser &parser, const MediaInfo &mediaInfo, bool is_dir,
                          const function<void(const string &errMsg, const HttpServerCookie::Ptr &cookie)> &callback) {
    //获取用户唯一id
    auto uid = parser.Params();
    auto path = parser.Url();

    //先根据http头中的cookie字段获取cookie
    HttpServerCookie::Ptr cookie = HttpCookieManager::Instance().getCookie(kCookieName, parser.getValues());
    //如果不是从http头中找到的cookie,我们让http客户端设置下cookie
    bool cookie_from_header = true;
    if (!cookie && !uid.empty()) {
        //客户端请求中无cookie,再根据该用户的用户id获取cookie
        cookie = HttpCookieManager::Instance().getCookieByUid(kCookieName, uid);
        cookie_from_header = false;
    }

    if (cookie) {
        //找到了cookie，对cookie上锁先
        auto lck = cookie->getLock();
        auto accessErr = (*cookie)[kAccessErrKey].get<string>();
        auto cookiePath = (*cookie)[kCookiePathKey].get<string>();
        auto is_hls = (*cookie)[kAccessHls].get<bool>();
        if (path.find(cookiePath) == 0) {
            //上次cookie是限定本目录
            if (accessErr.empty()) {
                //上次鉴权成功
                if(is_hls){
                    //如果播放的是hls，那么刷新hls的cookie
                    cookie->updateTime();
                    cookie_from_header = false;
                }
                callback("", cookie_from_header ? nullptr : cookie);
                return;
            }
            //上次鉴权失败，但是如果url参数发生变更，那么也重新鉴权下
            if (parser.Params().empty() || parser.Params() == cookie->getUid()) {
                //url参数未变，或者本来就没有url参数，那么判断本次请求为重复请求，无访问权限
                callback(accessErr, cookie_from_header ? nullptr : cookie);
                return;
            }
        }
        //如果url参数变了或者不是限定本目录，那么旧cookie失效，重新鉴权
        HttpCookieManager::Instance().delCookie(cookie);
    }

    bool is_hls = end_of(path,kHlsSuffix);
    //该用户从来未获取过cookie，这个时候我们广播是否允许该用户访问该http目录
    HttpSession::HttpAccessPathInvoker accessPathInvoker = [callback, uid, path, is_dir, is_hls]( const string &errMsg,
                                                                                                  const string &cookie_path_in,
                                                                                                  int cookieLifeSecond) {
        HttpServerCookie::Ptr cookie;
        if (cookieLifeSecond) {
            //本次鉴权设置了有效期，我们把鉴权结果缓存在cookie中
            string cookie_path = cookie_path_in;
            if (cookie_path.empty()) {
                //如果未设置鉴权目录，那么我们采用当前目录
                cookie_path = is_dir ? path : path.substr(0, path.rfind("/") + 1);
            }

            cookie = HttpCookieManager::Instance().addCookie(kCookieName, uid, cookieLifeSecond);
            //对cookie上锁
            auto lck = cookie->getLock();
            //记录用户能访问的路径
            (*cookie)[kCookiePathKey].set<string>(cookie_path);
            //记录能否访问
            (*cookie)[kAccessErrKey].set<string>(errMsg);
            //记录访问的是否为hls
            (*cookie)[kAccessHls].set<bool>(is_hls);
        }
        callback(errMsg, cookie);
    };

    if (is_hls && emitHlsPlayed(parser, mediaInfo, path, is_dir, accessPathInvoker, sender)) {
        //是hls的播放鉴权,拦截之
        return;
    }

    //事件未被拦截，则认为是http下载请求
    bool flag = NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastHttpAccess, parser, mediaInfo, path, is_dir, accessPathInvoker, sender);
    if (!flag) {
        //此事件无人监听，我们默认都有权限访问
        callback("", nullptr);
    }
}

/**
 * 发送404 Not Found
 */
static void sendNotFound(const HttpFileManager::invoker &cb) {
    GET_CONFIG(string,notFound,Http::kNotFound);
    cb("404 Not Found","text/html",StrCaseMap(),std::make_shared<HttpStringBody>(notFound));
}

/**
 * 拼接文件路径
 */
static string pathCat(const string &a, const string &b){
    if(a.back() == '/'){
        return a + b;
    }
    return a + '/' + b;
}

/**
 * 访问文件
 * @param sender 事件触发者
 * @param parser http请求
 * @param mediaInfo http url信息
 * @param strFile 文件绝对路径
 * @param cb 回调对象
 */
static void accessFile(TcpSession &sender, const Parser &parser, const MediaInfo &mediaInfo, const string &strFile, const HttpFileManager::invoker &cb) {
    if (!File::is_file(strFile.data())) {
        sendNotFound(cb);
        return;
    }
    //判断是否有权限访问该文件
    canAccessPath(sender, parser, mediaInfo, false, [cb, strFile, parser](const string &errMsg, const HttpServerCookie::Ptr &cookie) {
        if (!errMsg.empty()) {
            StrCaseMap headerOut;
            if (cookie) {
                headerOut["Set-Cookie"] = cookie->getCookie((*cookie)[kCookiePathKey].get<string>());
            }
            cb("401 Unauthorized", "text/html", headerOut, std::make_shared<HttpStringBody>(errMsg));
            return;
        }

        StrCaseMap httpHeader;
        if (cookie) {
            httpHeader["Set-Cookie"] = cookie->getCookie((*cookie)[kCookiePathKey].get<string>());
        }
        HttpSession::HttpResponseInvoker invoker = [&](const string &codeOut, const StrCaseMap &headerOut, const HttpBody::Ptr &body) {
            cb(codeOut.data(), getContentType(strFile.data()), headerOut, body);
        };
        invoker.responseFile(parser.getValues(), httpHeader, strFile);
    });
}

/**
 * 访问文件或文件夹
 * @param sender 事件触发者
 * @param parser http请求
 * @param cb 回调对象
 */
void HttpFileManager::onAccessPath(TcpSession &sender, Parser &parser, const HttpFileManager::invoker &cb) {
    auto fullUrl = string(HTTP_SCHEMA) + "://" + parser["Host"] + parser.FullUrl();
    MediaInfo mediaInfo(fullUrl);

    GET_CONFIG(bool, enableVhost, General::kEnableVhost);
    GET_CONFIG(string, rootPath, Http::kRootPath);
    auto strFile = File::absolutePath(enableVhost ? mediaInfo._vhost + parser.Url() : parser.Url(), rootPath);

    //访问的是文件夹
    if (File::is_dir(strFile.data())) {
        auto indexFile = searchIndexFile(strFile);
        if (!indexFile.empty()) {
            //发现该文件夹下有index文件
            strFile = pathCat(strFile, indexFile);
            parser.setUrl(pathCat(parser.Url(), indexFile));
            accessFile(sender, parser, mediaInfo, strFile, cb);
            return;
        }
        string strMenu;
        //生成文件夹菜单索引
        if (!makeFolderMenu(parser.Url(), strFile, strMenu)) {
            //文件夹不存在
            sendNotFound(cb);
            return;
        }
        //判断是否有权限访问该目录
        canAccessPath(sender, parser, mediaInfo, true, [strMenu, cb](const string &errMsg, const HttpServerCookie::Ptr &cookie) {
            if (!errMsg.empty()) {
                const_cast<string &>(strMenu) = errMsg;
            }
            StrCaseMap headerOut;
            if (cookie) {
                headerOut["Set-Cookie"] = cookie->getCookie((*cookie)[kCookiePathKey].get<string>());
            }
            cb(errMsg.empty() ? "200 OK" : "401 Unauthorized", "text/html", headerOut, std::make_shared<HttpStringBody>(strMenu));
        });
        return;
    }

    //访问的是文件
    accessFile(sender, parser, mediaInfo, strFile, cb);
};


////////////////////////////////////HttpResponseInvokerImp//////////////////////////////////////

void HttpResponseInvokerImp::operator()(const string &codeOut, const StrCaseMap &headerOut, const HttpBody::Ptr &body) const{
    if(_lambad){
        _lambad(codeOut,headerOut,body);
    }
}

void HttpResponseInvokerImp::operator()(const string &codeOut, const StrCaseMap &headerOut, const string &body) const{
    this->operator()(codeOut,headerOut,std::make_shared<HttpStringBody>(body));
}

HttpResponseInvokerImp::HttpResponseInvokerImp(const HttpResponseInvokerImp::HttpResponseInvokerLambda0 &lambda){
    _lambad = lambda;
}

HttpResponseInvokerImp::HttpResponseInvokerImp(const HttpResponseInvokerImp::HttpResponseInvokerLambda1 &lambda){
    if(!lambda){
        _lambad = nullptr;
        return;
    }
    _lambad = [lambda](const string &codeOut, const StrCaseMap &headerOut, const HttpBody::Ptr &body){
        string str;
        if(body && body->remainSize()){
            str = body->readData(body->remainSize())->toString();
        }
        lambda(codeOut,headerOut,str);
    };
}

void HttpResponseInvokerImp::responseFile(const StrCaseMap &requestHeader,
                                          const StrCaseMap &responseHeader,
                                          const string &filePath) const {
    StrCaseMap &httpHeader = const_cast<StrCaseMap&>(responseHeader);
    std::shared_ptr<FILE> fp(fopen(filePath.data(), "rb"), [](FILE *fp) {
        if (fp) {
            fclose(fp);
        }
    });

    if (!fp) {
        //打开文件失败
        GET_CONFIG(string,notFound,Http::kNotFound);
        GET_CONFIG(string,charSet,Http::kCharSet);

        auto strContentType = StrPrinter << "text/html; charset=" << charSet << endl;
        httpHeader["Content-Type"] = strContentType;
        (*this)("404 Not Found", httpHeader, notFound);
        return;
    }

    auto &strRange = const_cast<StrCaseMap &>(requestHeader)["Range"];
    int64_t iRangeStart = 0;
    int64_t iRangeEnd = 0 ;
    int64_t fileSize = HttpMultiFormBody::fileSize(fp.get());

    const char *pcHttpResult = NULL;
    if (strRange.size() == 0) {
        //全部下载
        pcHttpResult = "200 OK";
        iRangeEnd =  fileSize - 1;
    } else {
        //分节下载
        pcHttpResult = "206 Partial Content";
        iRangeStart = atoll(FindField(strRange.data(), "bytes=", "-").data());
        iRangeEnd = atoll(FindField(strRange.data(), "-", "\r\n").data());
        if (iRangeEnd == 0) {
            iRangeEnd = fileSize - 1;
        }
        //分节下载返回Content-Range头
        httpHeader.emplace("Content-Range", StrPrinter << "bytes " << iRangeStart << "-" << iRangeEnd << "/" << fileSize << endl);
    }

    //回复文件
    HttpBody::Ptr fileBody = std::make_shared<HttpFileBody>(fp, iRangeStart, iRangeEnd - iRangeStart + 1);
    (*this)(pcHttpResult, httpHeader, fileBody);
}

HttpResponseInvokerImp::operator bool(){
    return _lambad.operator bool();
}


}//namespace mediakit