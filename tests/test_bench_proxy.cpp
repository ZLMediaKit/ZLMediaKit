#include <map>
#include <signal.h>
#include <iostream>
#include "Util/CMD.h"
#include "Util/logger.h"
#include "Common/config.h"
#include "Player/PlayerProxy.h"
#include "Thread/WorkThreadPool.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

class CMD_main : public CMD {
public:
    CMD_main() {
        _parser.reset(new OptionParser(nullptr));

        (*_parser) << Option('l',/*该选项简称，如果是\x00则说明无简称*/
                             "level",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             to_string(LTrace).data(),/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "日志等级,LTrace~LError(0~4)",/*该选项说明文字*/
                             nullptr);


        (*_parser) << Option('t',/*该选项简称，如果是\x00则说明无简称*/
                             "threads",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             to_string(thread::hardware_concurrency()).data(),/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "启动事件触发线程数",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('i',/*该选项简称，如果是\x00则说明无简称*/
                             "in",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             nullptr,/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "拉流url,支持rtsp/rtmp/hls",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('c',/*该选项简称，如果是\x00则说明无简称*/
                             "count",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             "1000",/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "拉流拉流代理个数",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('d',/*该选项简称，如果是\x00则说明无简称*/
                             "delay",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             "50",/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "启动拉流代理间隔,单位毫秒",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('m',/*该选项简称，如果是\x00则说明无简称*/
                             "merge",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             "300",/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "合并写毫秒,合并写能提高性能",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('T',/*该选项简称，如果是\x00则说明无简称*/
                             "rtp",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             to_string((int) (Rtsp::RTP_TCP)).data(),/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "rtsp拉流方式,支持tcp/udp/multicast:0/1/2",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('D',/*该选项简称，如果是\x00则说明无简称*/
                             "demand",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             "1",/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "是否按需转协议，设置为1提高性能",/*该选项说明文字*/
                             nullptr);


    }

    ~CMD_main() override {}

    const char *description() const override {
        return "主程序命令参数";
    }
};

//此程序为zlm的拉流代理性能测试工具，用于测试拉流代理性能
int main(int argc, char *argv[]) {
    {
        CMD_main cmd_main;
        try {
            cmd_main.operator()(argc, argv);
        } catch (ExitException &) {
            return 0;
        } catch (std::exception &ex) {
            cout << ex.what() << endl;
            return -1;
        }

        int threads = cmd_main["threads"];
        LogLevel logLevel = (LogLevel) cmd_main["level"].as<int>();
        logLevel = MIN(MAX(logLevel, LTrace), LError);
        auto in_url = cmd_main["in"];
        auto rtp_type = cmd_main["rtp"].as<int>();
        auto delay_ms = cmd_main["delay"].as<int>();
        auto proxy_count = cmd_main["count"].as<int>();
        auto merge_ms = cmd_main["merge"].as<int>();
        auto demand = cmd_main["demand"].as<int>();

        //设置日志
        Logger::Instance().add(std::make_shared<ConsoleChannel>("ConsoleChannel", logLevel));
        //启动异步日志线程
        Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

        //设置线程数
        EventPollerPool::setPoolSize(threads);
        WorkThreadPool::setPoolSize(threads);

        //设置合并写
        mINI::Instance()[General::kMergeWriteMS] = merge_ms;
        mINI::Instance()[General::kRtspDemand] = demand;
        mINI::Instance()[General::kRtmpDemand] = demand;
        mINI::Instance()[General::kHlsDemand] = demand;
        mINI::Instance()[General::kTSDemand] = demand;
        mINI::Instance()[General::kFMP4Demand] = demand;

        map<string, PlayerProxy::Ptr> proxyMap;
        for (auto i = 0; i < proxy_count; ++i) {
            auto stream = to_string(i);
            PlayerProxy::Ptr player(new PlayerProxy(DEFAULT_VHOST, "live", stream, false, false));
            (*player)[kRtpType] = rtp_type;
            player->play(in_url);
            proxyMap.emplace(stream, player);
            //休眠后再启动下一个拉流代理，防止短时间海量链接
            if (delay_ms > 0) {
                usleep(1000 * delay_ms);
            }
        }

        static semaphore sem;
        signal(SIGINT, [](int) { sem.post(); });// 设置退出信号
        sem.wait();
    }
    return 0;
}

