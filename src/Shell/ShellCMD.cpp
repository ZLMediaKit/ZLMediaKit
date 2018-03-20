//
// Created by xzl on 2017/12/1.
//

#include "Util/CMD.h"
#include "Common/MediaSource.h"

using namespace ZL::Util;
using namespace ZL::Media;

namespace ZL {
namespace Shell {


class CMD_media: public CMD {
public:
    CMD_media(){
        _parser.reset(new OptionParser([](const std::shared_ptr<ostream> &stream,mINI &ini){
            MediaSource::for_each_media([&](const string &schema,
                                            const string &vhost,
                                            const string &app,
                                            const string &streamid,
                                            const MediaSource::Ptr &media){
                if(!ini["schema"].empty() && ini["schema"] != schema){
                    //筛选协议不匹配
                    return;
                }
                if(!ini["vhost"].empty() && ini["vhost"] != vhost){
                    //筛选虚拟主机不匹配
                    return;
                }
                if(!ini["app"].empty() && ini["app"] != app){
                    //筛选应用名不匹配
                    return;
                }
                if(!ini["stream"].empty() && ini["stream"] != streamid){
                    //流id不匹配
                    return;
                }
                if(ini.find("list") != ini.end()){
                    //列出源
                    (*stream) << "\t"
                              << schema << "/"
                              << vhost << "/"
                              << app << "/"
                              << streamid
                              << "\r\n";
                    return;
                }

                if(ini.find("kick") != ini.end()){
                    //踢出源
                    do{
                        if(!media) {
                            break;
                        }
                        if(!media->shutDown()) {
                            break;
                        }
                        (*stream) << "\t踢出成功:"
                                  << schema << "/"
                                  << vhost << "/"
                                  << app << "/"
                                  << streamid
                                  << "\r\n";
                        return;
                    }while(0);
                    (*stream) << "\t踢出失败:"
                              << schema << "/"
                              << vhost << "/"
                              << app << "/"
                              << streamid
                              << "\r\n";
                    return;
                }

            });
        }));
        (*_parser) << Option('k', "kick", Option::ArgNone,nullptr,false, "踢出媒体源", nullptr);
        (*_parser) << Option('l', "list", Option::ArgNone,nullptr,false, "列出媒体源", nullptr);
        (*_parser) << Option('S', "schema", Option::ArgRequired,nullptr,false, "协议筛选", nullptr);
        (*_parser) << Option('v', "vhost", Option::ArgRequired,nullptr,false, "虚拟主机筛选", nullptr);
        (*_parser) << Option('a', "app", Option::ArgRequired,nullptr,false, "应用名筛选", nullptr);
        (*_parser) << Option('s', "stream", Option::ArgRequired,nullptr,false, "流id筛选", nullptr);
    }
    virtual ~CMD_media() {}
    const char *description() const override {
        return "媒体源相关操作.";
    }
};

static onceToken s_token([]() {
    REGIST_CMD(media);
}, nullptr);


}/* namespace Shell */
} /* namespace ZL */