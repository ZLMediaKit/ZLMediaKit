/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <signal.h>
#include <string>
#include <iostream>
#include "Util/MD5.h"
#include "Util/File.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Poller/EventPoller.h"
#include "Http/HttpRequester.h"
#include "Http/HttpDownloader.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

int main(int argc, char *argv[]) {
    //设置退出信号处理函数
    static semaphore sem;
    signal(SIGINT, [](int) { sem.post(); });// 设置退出信号

    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    //加载证书，证书包含公钥和私钥
    SSL_Initor::Instance().loadCertificate((exeDir() + "ssl.p12").data());
    //信任某个自签名证书
    SSL_Initor::Instance().trustCertificate((exeDir() + "ssl.p12").data());
    //不忽略无效证书证书(例如自签名或过期证书)
    SSL_Initor::Instance().ignoreInvalidCertificate(false);

    ///////////////////////////////http downloader///////////////////////
    //下载器map
    map<string, HttpDownloader::Ptr> downloaderMap;
    //下载两个文件，一个是http下载，一个https下载
    auto urlList = {"http://www.baidu.com/img/baidu_resultlogo@2.png",
                    "https://www.baidu.com/img/baidu_resultlogo@2.png"};

    for (auto &url : urlList) {
        //创建下载器
        HttpDownloader::Ptr downloader(new HttpDownloader());
        downloader->setOnResult([](ErrCode code, const string &errMsg, const string &filePath) {
            DebugL << "=====================HttpDownloader result=======================";
            //下载结果回调
            if (code == Err_success) {
                //文件下载成功
                InfoL << "download file success:" << filePath;
            } else {
                //下载失败
                WarnL << "code:" << code << " msg:" << errMsg;
            }
        });
        //断点续传功能,开启后可能会遇到416的错误（因为上次文件已经下载完全）
        downloader->startDownload(url, exeDir() + MD5(url).hexdigest() + ".jpg", true);
        //下载器必须被强引用，否则作用域一失效就会导致对象销毁
        downloaderMap.emplace(url, downloader);
    }

    ///////////////////////////////http get///////////////////////
    //创建一个Http请求器
    HttpRequester::Ptr requesterGet(new HttpRequester());
    //使用GET方式请求
    requesterGet->setMethod("GET");
    //设置http请求头，我们假设设置cookie，当然你也可以设置其他http头
    requesterGet->addHeader("Cookie", "SESSIONID=e1aa89b3-f79f-4ac6-8ae2-0cea9ae8e2d7");
    //开启请求，该api会返回当前主机外网ip等信息
    requesterGet->startRequester("http://pv.sohu.com/cityjson?ie=utf-8",//url地址
                                 [](const SockException &ex,                                 //网络相关的失败信息，如果为空就代表成功
                                    const string &status,                                    //http回复的状态码，比如说200/404
                                    const HttpClient::HttpHeader &header,                    //http回复头
                                    const string &strRecvBody) {                              //http回复body
                                     DebugL << "=====================HttpRequester GET===========================";
                                     if (ex) {
                                         //网络相关的错误
                                         WarnL << "network err:" << ex.getErrCode() << " " << ex.what();
                                     } else {
                                         //打印http回复信息
                                         _StrPrinter printer;
                                         for (auto &pr: header) {
                                             printer << pr.first << ":" << pr.second << "\r\n";
                                         }
                                         InfoL << "status:" << status << "\r\n"
                                               << "header:\r\n" << (printer << endl)
                                               << "\r\nbody:" << strRecvBody;
                                     }
                                 });

    ///////////////////////////////http post///////////////////////
    //创建一个Http请求器
    HttpRequester::Ptr requesterPost(new HttpRequester());
    //使用POST方式请求
    requesterPost->setMethod("POST");
    //设置http请求头
    requesterPost->addHeader("X-Requested-With", "XMLHttpRequest");
    requesterPost->addHeader("Origin", "http://fanyi.baidu.com");
    //设置POST参数列表
    HttpArgs args;
    args["query"] = "test";
    args["from"] = "en";
    args["to"] = "zh";
    args["transtype"] = "translang";
    args["simple_means_flag"] = "3";
    requesterPost->setBody(args.make());
    //开启请求
    requesterPost->startRequester("http://fanyi.baidu.com/langdetect",//url地址
                                  [](const SockException &ex,                          //网络相关的失败信息，如果为空就代表成功
                                     const string &status,                             //http回复的状态码，比如说200/404
                                     const HttpClient::HttpHeader &header,             //http回复头
                                     const string &strRecvBody) {                       //http回复body
                                      DebugL << "=====================HttpRequester POST==========================";
                                      if (ex) {
                                          //网络相关的错误
                                          WarnL << "network err:" << ex.getErrCode() << " " << ex.what();
                                      } else {
                                          //打印http回复信息
                                          _StrPrinter printer;
                                          for (auto &pr: header) {
                                              printer << pr.first << ":" << pr.second << "\r\n";
                                          }
                                          InfoL << "status:" << status << "\r\n"
                                                << "header:\r\n" << (printer << endl)
                                                << "\r\nbody:" << strRecvBody;
                                      }
                                  });

    ///////////////////////////////http upload///////////////////////
    //创建一个Http请求器
    HttpRequester::Ptr requesterUploader(new HttpRequester());
    //使用POST方式请求
    requesterUploader->setMethod("POST");
    //设置http请求头
    HttpArgs argsUploader;
    argsUploader["query"] = "test";
    argsUploader["from"] = "en";
    argsUploader["to"] = "zh";
    argsUploader["transtype"] = "translang";
    argsUploader["simple_means_flag"] = "3";

    static string boundary = "0xKhTmLbOuNdArY";
    HttpMultiFormBody::Ptr body(new HttpMultiFormBody(argsUploader, exePath(), boundary));
    requesterUploader->setBody(body);
    requesterUploader->addHeader("Content-Type", HttpMultiFormBody::multiFormContentType(boundary));
    //开启请求
    requesterUploader->startRequester("http://fanyi.baidu.com/langdetect",//url地址
                                      [](const SockException &ex,                          //网络相关的失败信息，如果为空就代表成功
                                         const string &status,                             //http回复的状态码，比如说200/404
                                         const HttpClient::HttpHeader &header,             //http回复头
                                         const string &strRecvBody) {                       //http回复body
                                          DebugL << "=====================HttpRequester Uploader==========================";
                                          if (ex) {
                                              //网络相关的错误
                                              WarnL << "network err:" << ex.getErrCode() << " " << ex.what();
                                          } else {
                                              //打印http回复信息
                                              _StrPrinter printer;
                                              for (auto &pr: header) {
                                                  printer << pr.first << ":" << pr.second << "\r\n";
                                              }
                                              InfoL << "status:" << status << "\r\n"
                                                    << "header:\r\n" << (printer << endl)
                                                    << "\r\nbody:" << strRecvBody;
                                          }
                                      });

    sem.wait();
    return 0;
}

