#include "Http/HttpRequester.h"

int main() {
    auto requester = std::make_shared<mediakit::HttpRequester>();
    requester->setMethod("HEAD");

    requester->startRequester(
        "http://baidu.com",

        [](const toolkit::SockException &ex, const mediakit::Parser &parser) {
            if (ex) {
                PrintI("HEAD请求失败: %s", ex.what());
                return;
            }

            // 检查HTTP状态码
            if (parser.status() != "200") {
                PrintI("HEAD请求返回错误状态: %s", parser.status().c_str());
                return;
            }
            for (auto &header : parser.getHeader()) {
                PrintI("key=%s, val=%s", header.first.c_str(), header.second.c_str());
            }
        });
    getchar();
    return 0;
}