/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <cassert>
#include <cstring>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

#include "Common/config.h"
#include "Record/HlsMaker.h"
#include "Util/util.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

namespace {

size_t g_failed = 0;

// GET_CONFIG将配置值静态缓存，改动后须广播配置重载事件才会刷新
// GET_CONFIG caches config values statically; a reload broadcast is required to refresh them
void setProgramDateTime(bool enabled) {
    mINI::Instance()[Hls::kProgramDateTime] = enabled;
    NOTICE_EMIT(BroadcastReloadConfigArgs, Broadcast::kBroadcastReloadConfig);
}

void expect(bool ok, const string &what) {
    if (!ok) {
        ++g_failed;
        cout << "[FAIL] " << what << endl;
    } else {
        cout << "[ OK ] " << what << endl;
    }
}

// 捕获m3u8输出的HlsMaker，切片直接以序号命名
// HlsMaker that captures the m3u8 output; segments are simply named by index
class TestHlsMaker : public HlsMaker {
public:
    TestHlsMaker(float seg_duration, uint32_t seg_number)
        : HlsMaker(false, seg_duration, seg_number, false) {}

    // 喂入一个关键帧包，触发切片
    // Feed one key-frame packet to trigger segmentation
    void feedKeyFrame(uint64_t timestamp) {
        char payload[188] = { 0 };
        inputData(payload, sizeof(payload), timestamp, true);
    }

    // 喂入一个普通包，推进切片内的时间戳
    // Feed a normal packet to advance the timestamp inside the segment
    void feedData(uint64_t timestamp) {
        char payload[188] = { 0 };
        inputData(payload, sizeof(payload), timestamp, false);
    }

    void finish() { flushLastSegment(true); }

    string m3u8;

protected:
    string onOpenSegment(uint64_t index) override { return to_string(index) + ".ts"; }
    void onDelSegment(uint64_t index) override {}
    void onWriteInitSegment(const char *data, size_t len) override {}
    void onWriteSegment(const char *data, size_t len) override {}
    void onWriteHls(const string &data, bool include_delay) override {
        if (!include_delay) {
            m3u8 = data;
        }
    }
};

// 生成一份含3个切片的m3u8
// Produce an m3u8 containing 3 segments
string makeM3u8(float seg_duration = 1) {
    TestHlsMaker maker(seg_duration, 3);
    uint64_t stamp = 0;
    for (int i = 0; i < 3; ++i) {
        maker.feedKeyFrame(stamp);
        stamp += (uint64_t)(seg_duration * 1000);
        maker.feedData(stamp);
    }
    maker.finish();
    return maker.m3u8;
}

void test_tag_present_and_well_formed() {
    setProgramDateTime(true);
    auto m3u8 = makeM3u8();

    expect(m3u8.find("#EXT-X-PROGRAM-DATE-TIME:") != string::npos,
           "开启配置时m3u8应包含EXT-X-PROGRAM-DATE-TIME标签");

    // RFC 8216要求ISO 8601格式，且SHOULD带时区与毫秒
    // RFC 8216 requires ISO 8601, and SHOULD carry timezone and milliseconds
    regex re(R"(#EXT-X-PROGRAM-DATE-TIME:\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z)");
    expect(regex_search(m3u8, re), "标签取值应为带毫秒的UTC ISO 8601时间");
}

void test_tag_precedes_its_segment() {
    setProgramDateTime(true);
    auto m3u8 = makeM3u8();

    // 标签只作用于其后的第一个切片，必须紧邻对应的EXTINF之前
    // The tag applies only to the next segment, so it must sit right before its EXTINF
    size_t pos = 0, pairs = 0;
    while ((pos = m3u8.find("#EXT-X-PROGRAM-DATE-TIME:", pos)) != string::npos) {
        auto line_end = m3u8.find('\n', pos);
        expect(line_end != string::npos, "标签行应以换行结束");
        if (line_end == string::npos) {
            return;
        }
        expect(m3u8.compare(line_end + 1, strlen("#EXTINF:"), "#EXTINF:") == 0,
               "每个EXT-X-PROGRAM-DATE-TIME之后应紧跟#EXTINF");
        ++pairs;
        pos = line_end + 1;
    }
    expect(pairs == 3, "3个切片应各带1个EXT-X-PROGRAM-DATE-TIME标签, 实际: " + to_string(pairs));
}

void test_disabled_by_config() {
    setProgramDateTime(false);
    auto m3u8 = makeM3u8();

    expect(m3u8.find("#EXT-X-PROGRAM-DATE-TIME") == string::npos,
           "关闭配置时m3u8不应包含EXT-X-PROGRAM-DATE-TIME标签");
    expect(m3u8.find("#EXTINF:") != string::npos,
           "关闭配置时切片条目仍应正常输出");

    setProgramDateTime(true);
}

// 从m3u8中取出首个EXT-X-PROGRAM-DATE-TIME的取值
// Extract the first EXT-X-PROGRAM-DATE-TIME value out of the m3u8
string extractDateTime(const string &m3u8) {
    const string tag = "#EXT-X-PROGRAM-DATE-TIME:";
    auto pos = m3u8.find(tag);
    if (pos == string::npos) {
        return "";
    }
    auto begin = pos + tag.size();
    auto end = m3u8.find('\n', begin);
    return end == string::npos ? m3u8.substr(begin) : m3u8.substr(begin, end - begin);
}

// 将ISO 8601的UTC字符串解析回epoch秒，解析失败返回-1
// Parse an ISO 8601 UTC string back into epoch seconds; returns -1 on failure
time_t parseUtc(const string &str) {
    struct tm t;
    memset(&t, 0, sizeof(t));
    int msec = 0;
    if (sscanf(str.c_str(), "%d-%d-%dT%d:%d:%d.%d", &t.tm_year, &t.tm_mon, &t.tm_mday,
               &t.tm_hour, &t.tm_min, &t.tm_sec, &msec) != 7) {
        return (time_t)-1;
    }
    t.tm_year -= 1900;
    t.tm_mon -= 1;
#if defined(_WIN32)
    return _mkgmtime(&t);
#else
    return timegm(&t);
#endif
}

void test_value_is_utc_wall_clock() {
    setProgramDateTime(true);
    auto before = time(nullptr);
    auto m3u8 = makeM3u8();
    auto after = time(nullptr);

    auto value = extractDateTime(m3u8);
    expect(!value.empty(), "应能从m3u8中取到标签取值");
    if (value.empty()) {
        return;
    }
    expect(value.back() == 'Z', "取值应以Z结尾表明其为UTC: " + value);

    auto parsed = parseUtc(value);
    expect(parsed != (time_t)-1, "取值应可按ISO 8601解析: " + value);

    // 取值必须落在m3u8生成前后的UTC区间内。若实现把本地时间误标成Z
    // (最危险的一种回归)，在非UTC时区下偏差恰等于时区偏移，此断言即可捕获
    // The value must fall inside the UTC window around m3u8 generation. Should the
    // implementation mislabel local time as Z (the worst regression), the deviation equals
    // the timezone offset on a non-UTC host, which this assertion catches
    expect(parsed >= before - 2 && parsed <= after + 2,
           "取值应落在生成时刻的UTC区间内(可捕获把本地时间误标为UTC): " + value);
}

} // namespace

int main(int argc, char *argv[]) {
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setLevel(LWarn);

    test_value_is_utc_wall_clock();
    test_tag_present_and_well_formed();
    test_tag_precedes_its_segment();
    test_disabled_by_config();

    if (g_failed) {
        cout << "\n" << g_failed << " check(s) failed" << endl;
        return 1;
    }
    cout << "\nall checks passed" << endl;
    return 0;
}
