/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "HttpProtocolHint.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>

#include "Common/config.h"
#include "Util/util.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

namespace {

struct CachedAltSvc {
    HttpAltSvcEndpoint endpoint;
    uint64_t expire_at_ms = 0;
};

static mutex &altSvcMutex() {
    static mutex s_mtx;
    return s_mtx;
}

static unordered_map<string, CachedAltSvc> &altSvcCache() {
    static unordered_map<string, CachedAltSvc> s_cache;
    return s_cache;
}

static string makeOriginKey(string host, uint16_t port) {
    return strToLower(std::move(host)) + ":" + to_string(port);
}

static string trimQuotes(string value) {
    trim(value);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

static bool parseAltAuthority(const string &authority, const string &default_host,
                              uint16_t default_port, HttpAltSvcEndpoint *endpoint) {
    if (!endpoint) {
        return false;
    }

    auto value = trimQuotes(authority);
    if (value.empty()) {
        return false;
    }

    if (value[0] == ':') {
        endpoint->host = default_host;
        try {
            endpoint->port = static_cast<uint16_t>(stoul(value.substr(1)));
        } catch (...) {
            return false;
        }
        return endpoint->port != 0;
    }

    endpoint->host = value;
    endpoint->port = default_port;
    splitUrl(endpoint->host, endpoint->host, endpoint->port);
    return !endpoint->host.empty() && endpoint->port != 0;
}

static bool parseAltSvcItem(const string &item, const string &origin_host, uint16_t origin_port,
                            HttpAltSvcEndpoint *endpoint, uint64_t *expire_at_ms) {
    auto segments = split(item, ";");
    if (segments.empty()) {
        return false;
    }

    auto service = trim(segments[0]);
    auto eq = service.find('=');
    if (eq == string::npos) {
        return false;
    }

    auto alpn = trim(service.substr(0, eq));
    if (alpn != "h3" && alpn.find("h3-") != 0) {
        return false;
    }

    if (!parseAltAuthority(service.substr(eq + 1), origin_host, origin_port, endpoint)) {
        return false;
    }

    uint64_t max_age_sec = 86400;
    for (size_t i = 1; i < segments.size(); ++i) {
        auto part = trim(segments[i]);
        auto sep = part.find('=');
        if (sep == string::npos) {
            continue;
        }
        auto key = strToLower(trim(part.substr(0, sep)));
        auto value = trimQuotes(part.substr(sep + 1));
        if (key == "ma") {
            try {
                max_age_sec = static_cast<uint64_t>(stoull(value));
            } catch (...) {
                max_age_sec = 0;
            }
        }
    }

    if (expire_at_ms) {
        *expire_at_ms = getCurrentMillisecond() + max_age_sec * 1000;
    }
    return true;
}

} // namespace

void appendAltSvcHeader(StrCaseMap &headers) {
    GET_CONFIG(uint16_t, quic_port, Http::kQuicPort);
    if (!quic_port) {
        return;
    }
    headers.emplace("Alt-Svc", StrPrinter << "h3=\":"
                                          << quic_port
                                          << "\"; ma=86400");
}

void updateAltSvcCache(const string &origin_host, uint16_t origin_port, const string &alt_svc) {
    auto key = makeOriginKey(origin_host, origin_port);
    auto normalized = strToLower(trim(string(alt_svc)));

    lock_guard<mutex> lock(altSvcMutex());
    auto &cache = altSvcCache();
    if (normalized.empty() || normalized == "clear") {
        cache.erase(key);
        return;
    }

    auto items = split(alt_svc, ",");
    for (auto &item : items) {
        HttpAltSvcEndpoint endpoint;
        uint64_t expire_at_ms = 0;
        if (!parseAltSvcItem(item, origin_host, origin_port, &endpoint, &expire_at_ms)) {
            continue;
        }
        CachedAltSvc cached;
        cached.endpoint = std::move(endpoint);
        cached.expire_at_ms = expire_at_ms;
        cache[key] = std::move(cached);
        return;
    }
}

bool lookupAltSvc(const string &origin_host, uint16_t origin_port, HttpAltSvcEndpoint *endpoint) {
    if (!endpoint) {
        return false;
    }

    auto key = makeOriginKey(origin_host, origin_port);
    lock_guard<mutex> lock(altSvcMutex());
    auto &cache = altSvcCache();
    auto it = cache.find(key);
    if (it == cache.end()) {
        return false;
    }

    auto now = getCurrentMillisecond();
    if (it->second.expire_at_ms <= now) {
        cache.erase(it);
        return false;
    }

    *endpoint = it->second.endpoint;
    return true;
}

void eraseAltSvc(const string &origin_host, uint16_t origin_port) {
    lock_guard<mutex> lock(altSvcMutex());
    altSvcCache().erase(makeOriginKey(origin_host, origin_port));
}

} // namespace mediakit
