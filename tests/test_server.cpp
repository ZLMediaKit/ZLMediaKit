/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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

#include <map>
#include <signal.h>
#include <iostream>
#include "Common/config.h"
#include "Rtsp/UDPServer.h"
#include "Rtsp/RtspSession.h"
#include "Rtmp/RtmpSession.h"
#include "Http/HttpSession.h"
#include "Shell/ShellSession.h"
#include "Util/MD5.h"
#include "Rtmp/FlvMuxer.h"

#ifdef ENABLE_OPENSSL
#include "Util/SSLBox.h"
#include "Http/HttpsSession.h"
#endif//ENABLE_OPENSSL

#include "Util/File.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Network/TcpServer.h"
#include "Poller/EventPoller.h"
#include "Thread/WorkThreadPool.h"
#include "Device/PlayerProxy.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

#define REALM "realm_zlmedaikit"

static onceToken s_token([](){
	NoticeCenter::Instance().addListener(nullptr,Broadcast::kBroadcastOnGetRtspRealm,[](BroadcastOnGetRtspRealmArgs){
		if(string("1") == args.m_streamid ){
			// live/1需要认证
			EventPoller::Instance().async([invoker](){
				//该流需要认证，并且设置realm
				invoker(REALM);
			});
		}else{
			//我们异步执行invoker。
			//有时我们要查询redis或数据库来判断该流是否需要认证，通过invoker的方式可以做到完全异步
			EventPoller::Instance().async([invoker](){
				//该流我们不需要认证
				invoker("");
			});
		}
	});

	NoticeCenter::Instance().addListener(nullptr,Broadcast::kBroadcastOnRtspAuth,[](BroadcastOnRtspAuthArgs){
		InfoL << "用户：" << user_name <<  (must_no_encrypt ?  " Base64" : " MD5" )<< " 方式登录";
		string user = user_name;
		//假设我们异步读取数据库
		EventPoller::Instance().async([must_no_encrypt,invoker,user](){
			if(user == "test0"){
				//假设数据库保存的是明文
				invoker(false,"pwd0");
				return;
			}

			if(user == "test1"){
				//假设数据库保存的是密文
				auto encrypted_pwd = MD5(user + ":" + REALM + ":" + "pwd1").hexdigest();
				invoker(true,encrypted_pwd);
				return;
			}
			if(user == "test2" && must_no_encrypt){
				//假设登录的是test2,并且以base64方式登录，此时我们提供加密密码，那么会导致认证失败
				//可以通过这个方式屏蔽base64这种不安全的加密方式
				invoker(true,"pwd2");
				return;
			}

			//其他用户密码跟用户名一致
			invoker(false,user);
		});
	});


	NoticeCenter::Instance().addListener(nullptr,Broadcast::kBroadcastRtmpPublish,[](BroadcastRtmpPublishArgs){
        InfoL << args.m_vhost << " " << args.m_app << " " << args.m_streamid << " " << args.m_param_strs ;
        EventPoller::Instance().async([invoker](){
            invoker("");//鉴权成功
            //invoker("this is auth failed message");//鉴权失败
        });
    });

    NoticeCenter::Instance().addListener(nullptr,Broadcast::kBroadcastMediaPlayed,[](BroadcastMediaPlayedArgs){
        InfoL << args.m_schema << " " << args.m_vhost << " " << args.m_app << " " << args.m_streamid << " " << args.m_param_strs ;
        EventPoller::Instance().async([invoker](){
            invoker("");//鉴权成功
            //invoker("this is auth failed message");//鉴权失败
        });
    });

    NoticeCenter::Instance().addListener(nullptr,Broadcast::kBroadcastShellLogin,[](BroadcastShellLoginArgs){
        InfoL << "shell login:" << user_name << " " << passwd;
        EventPoller::Instance().async([invoker](){
            invoker("");//鉴权成功
            //invoker("this is auth failed message");//鉴权失败
        });
    });

    //此处用于测试rtmp保存为flv录像，保存在http根目录下
    NoticeCenter::Instance().addListener(nullptr,Broadcast::kBroadcastMediaChanged,[](BroadcastMediaChangedArgs){
        if(schema == RTMP_SCHEMA){
            static map<string,FlvRecorder::Ptr> s_mapFlvRecorder;
            static mutex s_mtxFlvRecorder;
            lock_guard<mutex> lck(s_mtxFlvRecorder);
            if(bRegist){
                GET_CONFIG_AND_REGISTER(string,http_root,Http::kRootPath);
                auto path = http_root + "/" + vhost + "/" + app + "/" + stream + "_" + to_string(time(NULL)) + ".flv";
                FlvRecorder::Ptr recorder(new FlvRecorder);
                try{
                    recorder->startRecord(dynamic_pointer_cast<RtmpMediaSource>(sender.shared_from_this()),path);
                    s_mapFlvRecorder[vhost + "/" + app + "/" + stream] = recorder;
                }catch(std::exception &ex){
                    WarnL << ex.what();
                }
            }else{
                s_mapFlvRecorder.erase(vhost + "/" + app + "/" + stream);
            }
        }
    });


}, nullptr);

#if !defined(SIGHUP)
#define SIGHUP 1
#endif

int main(int argc,char *argv[]) {
    //设置退出信号处理函数
    signal(SIGINT, [](int) { EventPoller::Instance().shutdown(); });
    signal(SIGHUP, [](int) { loadIniConfig(); });

    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
    //加载配置文件，如果配置文件不存在就创建一个
    loadIniConfig();
    {
        //这里是拉流地址，支持rtmp/rtsp协议，负载必须是H264+AAC
        //如果是其他不识别的音视频将会被忽略(譬如说h264+adpcm转发后会去除音频)
        auto urlList = {"rtmp://live.hkstv.hk.lxdns.com/live/hks",
                        "rtmp://live.hkstv.hk.lxdns.com/live/hks"
                //rtsp链接支持输入用户名密码
                /*"rtsp://admin:jzan123456@192.168.0.122/"*/};
        map<string, PlayerProxy::Ptr> proxyMap;
        int i = 0;
        for (auto &url : urlList) {
            //PlayerProxy构造函数前两个参数分别为应用名（app）,流id（streamId）
            //比如说应用为live，流id为0，那么直播地址为:
            //http://127.0.0.1/live/0/hls.m3u8
            //rtsp://127.0.0.1/live/0
            //rtmp://127.0.0.1/live/0
            //录像地址为(当然vlc不支持这么多级的rtmp url，可以用test_player测试rtmp点播):
            //http://127.0.0.1/record/live/0/2017-04-11/11-09-38.mp4
            //rtsp://127.0.0.1/record/live/0/2017-04-11/11-09-38.mp4
            //rtmp://127.0.0.1/record/live/0/2017-04-11/11-09-38.mp4
            PlayerProxy::Ptr player(new PlayerProxy(DEFAULT_VHOST, "live", to_string(i).data()));
            //指定RTP over TCP(播放rtsp时有效)
            (*player)[RtspPlayer::kRtpType] = PlayerBase::RTP_TCP;
            //开始播放，如果播放失败或者播放中止，将会自动重试若干次，重试次数在配置文件中配置，默认一直重试
            player->play(url);
            //需要保存PlayerProxy，否则作用域结束就会销毁该对象
            proxyMap.emplace(to_string(i), player);
            ++i;
        }

#ifdef ENABLE_OPENSSL
        //请把证书"test_server.pem"放置在本程序可执行程序同目录下
        try {
            //加载证书，证书包含公钥和私钥
            SSL_Initor::Instance().loadServerPem((exePath() + ".pem").data());
        } catch (...) {
            ErrorL << "请把证书:" << (exeName() + ".pem") << "放置在本程序可执行程序同目录下:" << exeDir() << endl;
            proxyMap.clear();
            return 0;
        }
#endif //ENABLE_OPENSSL

        uint16_t shellPort = mINI::Instance()[Shell::kPort];
        uint16_t rtspPort = mINI::Instance()[Rtsp::kPort];
        uint16_t rtmpPort = mINI::Instance()[Rtmp::kPort];
        uint16_t httpPort = mINI::Instance()[Http::kPort];
        uint16_t httpsPort = mINI::Instance()[Http::kSSLPort];

        //简单的telnet服务器，可用于服务器调试，但是不能使用23端口，否则telnet上了莫名其妙的现象
        //测试方法:telnet 127.0.0.1 9000
        TcpServer::Ptr shellSrv(new TcpServer());
        TcpServer::Ptr rtspSrv(new TcpServer());
        TcpServer::Ptr rtmpSrv(new TcpServer());
        TcpServer::Ptr httpSrv(new TcpServer());

        shellSrv->start<ShellSession>(shellPort);
        rtspSrv->start<RtspSession>(rtspPort);//默认554
        rtmpSrv->start<RtmpSession>(rtmpPort);//默认1935
        httpSrv->start<HttpSession>(httpPort);//默认80

#ifdef ENABLE_OPENSSL
        //如果支持ssl，还可以开启https服务器
        TcpServer::Ptr httpsSrv(new TcpServer());
        httpsSrv->start<HttpsSession>(httpsPort);//默认443
#endif //ENABLE_OPENSSL

        NoticeCenter::Instance().addListener(ReloadConfigTag,Broadcast::kBroadcastReloadConfig,[&](BroadcastReloadConfigArgs){
            //重新创建服务器
            if(shellPort != mINI::Instance()[Shell::kPort].as<uint16_t>()){
                shellPort = mINI::Instance()[Shell::kPort];
                shellSrv->start<ShellSession>(shellPort);
                InfoL << "重启shell服务器:" << shellPort;
            }
            if(rtspPort != mINI::Instance()[Rtsp::kPort].as<uint16_t>()){
                rtspPort = mINI::Instance()[Rtsp::kPort];
                rtspSrv->start<RtspSession>(rtspPort);
                InfoL << "重启rtsp服务器" << rtspPort;
            }
            if(rtmpPort != mINI::Instance()[Rtmp::kPort].as<uint16_t>()){
                rtmpPort = mINI::Instance()[Rtmp::kPort];
                rtmpSrv->start<RtmpSession>(rtmpPort);
                InfoL << "重启rtmp服务器" << rtmpPort;
            }
            if(httpPort != mINI::Instance()[Http::kPort].as<uint16_t>()){
                httpPort = mINI::Instance()[Http::kPort];
                httpSrv->start<HttpSession>(httpPort);
                InfoL << "重启http服务器" << httpPort;
            }
#ifdef ENABLE_OPENSSL
            if(httpsPort != mINI::Instance()[Http::kSSLPort].as<uint16_t>()){
                httpsPort = mINI::Instance()[Http::kSSLPort];
                httpsSrv->start<HttpsSession>(httpsPort);
                InfoL << "重启https服务器" << httpsPort;
            }
#endif //ENABLE_OPENSSL
        });

        EventPoller::Instance().runLoop();
    }//设置作用域，作用域结束后会销毁临时变量；省去手动注销服务器


	//rtsp服务器用到udp端口分配器了
	UDPServer::Destory();
	//TcpServer用到了WorkThreadPool
	WorkThreadPool::Destory();
	EventPoller::Destory();
    AsyncTaskThread::Destory();
    Logger::Destory();
	return 0;
}

