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
using namespace ZL::Util;
using namespace ZL::Http;
using namespace ZL::Poller;
using namespace ZL::Network;


int main(int argc,char *argv[]){
    //设置退出信号处理函数
    signal(SIGINT, [](int){EventPoller::Instance().shutdown();});
    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
	///////////////////////////////http downloader///////////////////////
	//下载器map
	map<string,HttpDownloader::Ptr> downloaderMap;
	//下载两个文件，一个是http下载，一个https下载
	auto urlList = {"http://img3.imgtn.bdimg.com/it/u=158031390,1321729164&fm=214&gp=0.jpg",
					"https://ss0.bdstatic.com/70cFvHSh_Q1YnxGkpoWK1HF6hhy/it/u=931786003,1029770543&fm=27&gp=0.jpg"};

	for(auto &url : urlList){
		//创建下载器
		HttpDownloader::Ptr downloader(new HttpDownloader());
		downloader->setOnResult([](ErrCode code,const char *errMsg,const char *filePath){
			DebugL << "=====================HttpDownloader result=======================";
			//下载结果回调
			if(code == Err_success){
				//文件下载成功
				InfoL << "download file success:" << filePath;
			}else{
				//下载失败
				WarnL << "code:" << code << " msg:" << errMsg;
			}
		});
		//断点续传功能,开启后可能会遇到416的错误（因为上次文件已经下载完全）
		downloader->startDownload(url,exeDir() + MD5(url).hexdigest() + ".jpg",true);
		//下载器必须被强引用，否则作用域一失效就会导致对象销毁
		downloaderMap.emplace(url,downloader);
	}

	///////////////////////////////http get///////////////////////
	//创建一个Http请求器
	HttpRequester::Ptr requesterGet(new HttpRequester());
	//使用GET方式请求
	requesterGet->setMethod("GET");
	//设置http请求头，我们假设设置cookie，当然你也可以设置其他http头
	requesterGet->addHeader("Cookie","SESSIONID=e1aa89b3-f79f-4ac6-8ae2-0cea9ae8e2d7");
	//开启请求，该api会返回当前主机外网ip等信息
	requesterGet->startRequester("http://pv.sohu.com/cityjson?ie=utf-8",//url地址
			[](const SockException &ex,                                 //网络相关的失败信息，如果为空就代表成功
			   const string &status,                                    //http回复的状态码，比如说200/404
			   const HttpClient::HttpHeader &header,                    //http回复头
			   const string &strRecvBody){                              //http回复body
		DebugL << "=====================HttpRequester GET===========================";
		if(ex){
			//网络相关的错误
			WarnL << "network err:" << ex.getErrCode() << " " << ex.what();
		}else{
			//打印http回复信息
			_StrPrinter printer;
			for(auto &pr: header){
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
	requesterPost->addHeader("X-Requested-With","XMLHttpRequest");
	requesterPost->addHeader("Origin","http://fanyi.baidu.com");
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
					const string &strRecvBody){                       //http回复body
		DebugL << "=====================HttpRequester POST==========================";
		if(ex){
			//网络相关的错误
			WarnL << "network err:" << ex.getErrCode() << " " << ex.what();
		} else {
			//打印http回复信息
			_StrPrinter printer;
			for(auto &pr: header){
				printer << pr.first << ":" << pr.second << "\r\n";
			}
			InfoL << "status:" << status << "\r\n"
				  << "header:\r\n" << (printer << endl)
				  << "\r\nbody:" << strRecvBody;
		}
	});

	//事件轮询
	EventPoller::Instance().runLoop();
	//清空下载器
	downloaderMap.clear();
	requesterGet.reset();
	requesterPost.reset();
	//程序开始退出
	EventPoller::Destory();
	AsyncTaskThread::Destory();
	Logger::Destory();
	return 0;
}

