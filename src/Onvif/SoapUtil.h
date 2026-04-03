/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_SOAPUTIL_H
#define ZLMEDIAKIT_SOAPUTIL_H

#include <map>
#include <memory>
#include <functional>
#include <initializer_list>
#include "Common/Parser.h"
#include "Network/Socket.h"
#include "pugixml.hpp"

struct xml_string_writer : pugi::xml_writer {
    std::string result;
    virtual void write(const void *data, size_t size) {
        result.append(static_cast<const char *>(data), size);
    }
};

template<size_t sz, typename ...ARGS>
std::string print_to_string(const char (&str_fmt)[sz], ARGS &&...args) {
    std::string ret;
    //鉴权%s长度再减去\0长度
    ret.resize(2 * sizeof(str_fmt));
    //string的真实内存大小必定比size大一个字节(用于存放\0)
    auto size = snprintf((char *) ret.data(), ret.size() + 1, str_fmt, std::forward<ARGS>(args)...);
    ret.resize(size);
    return ret;
}

class SoapObject;

class SoapErr {
public:
    SoapErr(std::string url,
            std::string action,
            toolkit::SockException ex,
            const mediakit::Parser &parser,
            std::string err = "");

    operator std::string() const;
    operator bool() const;
    bool empty() const;
    int httpCode() const;

private:
    std::string _url;
    std::string _action;
    toolkit::SockException _net_err;
    int _http_code = 200;
    std::string _http_msg;
    std::string _other_err;
};

std::ostream& operator<<(std::ostream& sout, const SoapErr &err);

class SoapUtil {
public:
    static std::string createDiscoveryString(const std::string &uuid = "");
    static std::string createUuidString();
    static std::string createSoapRequest(const std::string &body, const std::string &user_name = "", const std::string &passwd = "");

    using SoapRequestCB = std::function<void(const SoapObject &node, const SoapErr &err)>;
    static void sendSoapRequest(const std::string &url, const std::string &action, const std::string &body,
                                const SoapRequestCB &func = nullptr, float timeout_sec = 10);


    using onGetProfilesResponseTuple = std::tuple<std::string/*profile_name*/, std::string /*codec*/, int /*width*/, int /*height*/>;
    using onGetProfilesResponse = std::function<void(const SoapErr &err,
                                                     const std::vector<onGetProfilesResponseTuple> &profile)>;

    /**
     * 获取profile
     * @param is_media2 是否为media2访问方式
     * @param media_url media服务访问地址
     * @param user_name 用户名
     * @param pwd 密码
     * @param cb 回调, 高分辨率的profile在前
     */
    static void sendGetProfiles(bool is_media2, const std::string &media_url, const std::string &user_name,
                                const std::string &pwd, const onGetProfilesResponse &cb);

    /**
     * 获取设备信息
     * @param device_service device_service服务访问地址
     * @param user_name 用户名
     * @param pwd 密码
     * @param cb 回调
     */
    static void sendGetDeviceInformation(const std::string &device_service, const std::string &user_name,
                                         const std::string &pwd, SoapRequestCB cb);


    using onGetServicesResponseMap = std::map<std::string/*ns*/, std::string/*xaddr*/, mediakit::StrCaseCompare>;
    using onGetServicesResponse = std::function<void(const SoapErr &err, onGetServicesResponseMap &val)>;

    /**
     * 获取服务url地址
     * @param device_service device_service服务访问地址
     * @param ns_filter 刷选的服务的命名空间
     * @param user_name 用户名
     * @param pwd 密码
     * @param cb 回调
     */
    static void sendGetServices(const std::string &device_service, const std::initializer_list<std::string> &ns_filter,
                                const std::string &user_name, const std::string &pwd, const onGetServicesResponse &cb);


    using onGetStreamUriResponse = std::function<void(const SoapErr &err, const std::string &uri)>;

    /**
     * 获取rtsp播放url
     * @param is_media2 是否为media2方式
     * @param media_url media或media2服务访问地址
     * @param profile sendGetProfiles接口获取的分辨率方案
     * @param user_name 用户名
     * @param pwd 密码
     * @param cb 回调
     */
    static void sendGetStreamUri(bool is_media2, const std::string &media_url, const std::string &profile,
                                 const std::string &user_name, const std::string &pwd,
                                 const onGetStreamUriResponse &cb);

    using GetStreamUriRetryInvoker = std::function<void(const std::string &user_name, const std::string &pwd)>;
    using AsyncGetStreamUriCB = std::function<void(const SoapErr &err, const GetStreamUriRetryInvoker &invoker,
                                                   int retry_count, const std::string &url)>;

    /**
     * 异步获取播放url
     * @param onvif_url 设备搜索时返回的url
     * @param cb 回调
     */
    static void asyncGetStreamUri(const std::string &onvif_url, const AsyncGetStreamUriCB &cb);

private:
    SoapUtil() = delete;
    ~SoapUtil() = delete;
};

class SoapObject {
public:
    using Ptr = std::shared_ptr<SoapObject>;

    SoapObject(const pugi::xml_node &node, const SoapObject &ref);
    SoapObject();
    operator bool () const;
    void load(const char *data, size_t len);
    SoapObject operator[](const std::string &path) const;

    template<size_t sz>
    SoapObject operator[](const char (&path)[sz]) const{
        return (*this)[std::string(path, sz - 1)];
    }

    SoapObject operator[](size_t index) const;
    std::string as_string() const;
    pugi::xml_node as_xml() const;

private:
    SoapObject(std::shared_ptr<pugi::xml_node> node);

private:
    std::shared_ptr<pugi::xml_node> _root;
};



#endif //ZLMEDIAKIT_SOAPUTIL_H
