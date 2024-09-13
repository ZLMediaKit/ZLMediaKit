/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <signal.h>
#include <string>
#include <iostream>
#include "Util/MD5.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Poller/EventPoller.h"
#include "Http/HttpRequester.h"
#include "Http/HttpDownloader.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

int main(int argc, char *argv[]) {
    // 设置退出信号处理函数  [AUTO-TRANSLATED:4f047770]
    // Set the exit signal processing function
    static semaphore sem;
    signal(SIGINT, [](int) { sem.post(); });// 设置退出信号

    // 设置日志  [AUTO-TRANSLATED:50372045]
    // Set the log
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    // 加载证书，证书包含公钥和私钥  [AUTO-TRANSLATED:fce78641]
    // Load the certificate, the certificate contains the public key and private key
    SSL_Initor::Instance().loadCertificate((exeDir() + "ssl.p12").data());
    // 信任某个自签名证书  [AUTO-TRANSLATED:6815fc55]
    // Trust a self-signed certificate
    SSL_Initor::Instance().trustCertificate((exeDir() + "ssl.p12").data());
    // 不忽略无效证书证书(例如自签名或过期证书)  [AUTO-TRANSLATED:ee4a34c4]
    // Do not ignore invalid certificates (such as self-signed or expired certificates)
    SSL_Initor::Instance().ignoreInvalidCertificate(false);

    ///////////////////////////////http downloader///////////////////////
    // 下载器map  [AUTO-TRANSLATED:d8583648]
    // Downloader map
    map<string, HttpDownloader::Ptr> downloaderMap;
    // 下载两个文件，一个是http下载，一个https下载  [AUTO-TRANSLATED:d6108dc2]
    // Download two files, one is HTTP download, and the other is HTTPS download
    auto urlList = {"http://www.baidu.com/img/baidu_resultlogo@2.png",
                    "https://www.baidu.com/img/baidu_resultlogo@2.png"};

    for (auto &url : urlList) {
        // 创建下载器  [AUTO-TRANSLATED:e67aa738]
        // Create a downloader
        HttpDownloader::Ptr downloader(new HttpDownloader());
        downloader->setOnResult([](const SockException &ex, const string &filePath) {
            DebugL << "=====================HttpDownloader result=======================";
            // 下载结果回调  [AUTO-TRANSLATED:a7f68894]
            // Download result callback
            if (!ex) {
                // 文件下载成功  [AUTO-TRANSLATED:d753c6d3]
                // File download successful
                InfoL << "download file success:" << filePath;
            } else {
                // 下载失败  [AUTO-TRANSLATED:b5ee74cc]
                // Download failed
                WarnL << "code:" << ex.getErrCode() << " msg:" << ex.what();
            }
        });
        // 断点续传功能,开启后可能会遇到416的错误（因为上次文件已经下载完全）  [AUTO-TRANSLATED:d4feaecb]
        // Resume function, enabling it may encounter a 416 error (because the file was downloaded completely last time)
        downloader->startDownload(url, exeDir() + MD5(url).hexdigest() + ".jpg", true);
        // 下载器必须被强引用，否则作用域一失效就会导致对象销毁  [AUTO-TRANSLATED:16e93227]
        // The downloader must be strongly referenced, otherwise, it will be destroyed when the scope is invalid
        downloaderMap.emplace(url, downloader);
    }

    ///////////////////////////////http get///////////////////////
    // 创建一个Http请求器  [AUTO-TRANSLATED:0d451bc1]
    // Create an HTTP requestor
    HttpRequester::Ptr requesterGet(new HttpRequester());
    // 使用GET方式请求  [AUTO-TRANSLATED:3f701c92]
    // Use the GET method to request
    requesterGet->setMethod("GET");
    // 设置http请求头，我们假设设置cookie，当然你也可以设置其他http头  [AUTO-TRANSLATED:233d2c3f]
    // Set the HTTP request header, we assume setting the cookie, of course, you can also set other HTTP headers
    requesterGet->addHeader("Cookie", "SESSIONID=e1aa89b3-f79f-4ac6-8ae2-0cea9ae8e2d7");
    // 开启请求，该api会返回当前主机外网ip等信息  [AUTO-TRANSLATED:efebf262]
    // Start the request, this API will return the current host's external network IP and other information
    requesterGet->startRequester("http://pv.sohu.com/cityjson?ie=utf-8",//url地址
                                 [](const SockException &ex,                                 //网络相关的失败信息，如果为空就代表成功
                                    const Parser &parser) {                              //http回复body
                                     DebugL << "=====================HttpRequester GET===========================";
                                     if (ex) {
                                         // 网络相关的错误  [AUTO-TRANSLATED:ae96dbe9]
                                         // Network-related errors
                                         WarnL << "network err:" << ex.getErrCode() << " " << ex.what();
                                     } else {
                                         // 打印http回复信息  [AUTO-TRANSLATED:d4611ce2]
                                         // Print HTTP response information
                                         _StrPrinter printer;
                                         for (auto &pr: parser.getHeader()) {
                                             printer << pr.first << ":" << pr.second << "\r\n";
                                         }
                                         InfoL << "status:" << parser.status() << "\r\n"
                                               << "header:\r\n" << (printer << endl)
                                               << "\r\nbody:" << parser.content();
                                     }
                                 });

    ///////////////////////////////http post///////////////////////
    // 创建一个Http请求器  [AUTO-TRANSLATED:0d451bc1]
    // Create an HTTP requestor
    HttpRequester::Ptr requesterPost(new HttpRequester());
    // 使用POST方式请求  [AUTO-TRANSLATED:dc6266f1]
    // Use the POST method to request
    requesterPost->setMethod("POST");
    // 设置http请求头  [AUTO-TRANSLATED:d934a806]
    // Set the HTTP request header
    requesterPost->addHeader("X-Requested-With", "XMLHttpRequest");
    requesterPost->addHeader("Origin", "http://fanyi.baidu.com");
    // 设置POST参数列表  [AUTO-TRANSLATED:5378230d]
    // Set the POST parameter list
    HttpArgs args;
    args["query"] = "test";
    args["from"] = "en";
    args["to"] = "zh";
    args["transtype"] = "translang";
    args["simple_means_flag"] = "3";
    requesterPost->setBody(args.make());
    // 开启请求  [AUTO-TRANSLATED:ccb4bc7f]
    // Start the request
    requesterPost->startRequester("http://fanyi.baidu.com/langdetect",//url地址
                                  [](const SockException &ex,                          //网络相关的失败信息，如果为空就代表成功
                                     const Parser &parser) {                       //http回复body
                                      DebugL << "=====================HttpRequester POST==========================";
                                      if (ex) {
                                          // 网络相关的错误  [AUTO-TRANSLATED:ae96dbe9]
                                          // Network-related errors
                                          WarnL << "network err:" << ex.getErrCode() << " " << ex.what();
                                      } else {
                                          // 打印http回复信息  [AUTO-TRANSLATED:d4611ce2]
                                          // Print HTTP response information
                                          _StrPrinter printer;
                                          for (auto &pr: parser.getHeader()) {
                                              printer << pr.first << ":" << pr.second << "\r\n";
                                          }
                                          InfoL << "status:" << parser.status() << "\r\n"
                                                << "header:\r\n" << (printer << endl)
                                                << "\r\nbody:" << parser.content();
                                      }
                                  });

    ///////////////////////////////http upload///////////////////////
    // 创建一个Http请求器  [AUTO-TRANSLATED:0d451bc1]
    // Create an HTTP requestor
    HttpRequester::Ptr requesterUploader(new HttpRequester());
    // 使用POST方式请求  [AUTO-TRANSLATED:dc6266f1]
    // Use the POST method to request
    requesterUploader->setMethod("POST");
    // 设置http请求头  [AUTO-TRANSLATED:d934a806]
    // Set the HTTP request header
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
    // 开启请求  [AUTO-TRANSLATED:ccb4bc7f]
    // Start the request
    requesterUploader->startRequester("http://fanyi.baidu.com/langdetect",//url地址
                                      [](const SockException &ex,                          //网络相关的失败信息，如果为空就代表成功
                                         const Parser &parser) {                       //http回复body
                                          DebugL << "=====================HttpRequester Uploader==========================";
                                          if (ex) {
                                              // 网络相关的错误  [AUTO-TRANSLATED:ae96dbe9]
                                              // Network-related errors
                                              WarnL << "network err:" << ex.getErrCode() << " " << ex.what();
                                          } else {
                                              // 打印http回复信息  [AUTO-TRANSLATED:d4611ce2]
                                              // Print HTTP response information
                                              _StrPrinter printer;
                                              for (auto &pr: parser.getHeader()) {
                                                  printer << pr.first << ":" << pr.second << "\r\n";
                                              }
                                              InfoL << "status:" << parser.status() << "\r\n"
                                                    << "header:\r\n" << (printer << endl)
                                                    << "\r\nbody:" << parser.content();
                                          }
                                      });

    sem.wait();
    return 0;
}

