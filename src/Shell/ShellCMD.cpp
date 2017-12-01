//
// Created by xzl on 2017/12/1.
//

#include "Util/CMD.h"
#include "Rtsp/RtspMediaSource.h"
#include "Rtmp/RtmpMediaSource.h"

using namespace ZL::Util;
using namespace ZL::Rtsp;
using namespace ZL::Rtmp;

namespace ZL {
namespace Shell {

class CMD_rtsp: public CMD {
public:
    CMD_rtsp(){
        _parser.reset(new OptionParser(nullptr));
        (*_parser) << Option('l', "list", Option::ArgNone, nullptr,false, "list all media source of rtsp",
                             [](const std::shared_ptr<ostream> &stream, const string &arg) {
                                 auto mediaSet = RtspMediaSource::getMediaSet();
                                 for (auto &src : mediaSet) {
                                     (*stream) << "\t" << src << "\r\n";
                                 }
                                 return false;
                             });
    }
    virtual ~CMD_rtsp() {}
    const char *description() const override {
        return "查看rtsp服务器相关信息.";
    }
};
class CMD_rtmp: public CMD {
public:
    CMD_rtmp(){
        _parser.reset(new OptionParser(nullptr));
        (*_parser) << Option('l', "list", Option::ArgNone,nullptr,false, "list all media source of rtmp",
                             [](const std::shared_ptr<ostream> &stream, const string &arg) {
                                 auto mediaSet = RtmpMediaSource::getMediaSet();
                                 for (auto &src : mediaSet) {
                                     (*stream) << "\t" << src << "\r\n";
                                 }
                                 return false;
                             });
    }
    virtual ~CMD_rtmp() {}
    const char *description() const override {
        return "查看rtmp服务器相关信息.";
    }
};

static onceToken s_token([]() {
    REGIST_CMD(rtmp);
    REGIST_CMD(rtsp);
}, nullptr);


}/* namespace Shell */
} /* namespace ZL */