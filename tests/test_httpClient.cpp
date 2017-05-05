//============================================================================
// Name        : main.cpp
// Author      : 熊子良
// Version     :
//============================================================================

#include <signal.h>
#include <unistd.h>
#include <iostream>
#include "Http/HttpDownloader.h"
#include "Http/HttpRequester.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Util/File.h"
#include "Poller/EventPoller.h"
#include <list>

using namespace std;
using namespace ZL::Util;
using namespace ZL::Http;
using namespace ZL::Poller;
using namespace ZL::Network;

void programExit(int arg) {
	EventPoller::Instance().shutdown();
}
int main(int argc,char *argv[]){
	signal(SIGINT, programExit);
	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));

	///////////////////////////////http downloader///////////////////////
	list<HttpDownloader::Ptr> downloaderList;
	auto urlList = {"http://img3.imgtn.bdimg.com/it/u=158031390,1321729164&fm=214&gp=0.jpg",
					"https://media-cdn.tripadvisor.com/media/photo-s/06/c3/2f/64/de-notre-chambre.jpg"};
	int i=0;
	for(auto url : urlList){
		HttpDownloader::Ptr downloader(new HttpDownloader());
		downloader->setOnResult([](ErrCode code,const char *errMsg,const char *filePath){
			if(code == Err_success){
				InfoL << "download file success:" << filePath;
			}else{
				WarnL << "code:" << code << " msg:" << errMsg;
			}
		});
		//断点续传功能,开启后可能会遇到416的错误（因为上次文件已经下载完全）
		downloader->startDownload(url,exeDir() +  to_string(i++) + ".jpg",true);
		downloaderList.push_back(downloader);
	}

	///////////////////////////////http get///////////////////////
	HttpRequester::Ptr requesterGet(new HttpRequester());
	requesterGet->setMethod("GET");
	//设置http头，我们假设设置cookie
	requesterGet->addHeader("Cookie","SESSIONID=e1aa89b3-f79f-4ac6-8ae2-0cea9ae8e2d7");
	requesterGet->startRequester("http://pv.sohu.com/cityjson?ie=utf-8",
			[](const SockException &ex,
			   const string &status,
			   const HttpClient::HttpHeader &header,
			   const string &strRecvBody){
		if(ex){
			WarnL << "network err:" << ex.getErrCode() << " " << ex.what();
		}else{
			_StrPrinter printer;
			for(auto &pr: header){
				printer << pr.first << ":" << pr.second << "\r\n";
			}
			InfoL << "\r\nhttp status:" << status << "\r\n\r\n"
					<< "header:" << (printer << endl)
					<< "\r\nbody:" << strRecvBody;
		}
	});

	///////////////////////////////http post///////////////////////
	HttpRequester::Ptr requesterPost(new HttpRequester());
	requesterPost->setMethod("POST");
	HttpArgs args;
	args["query"] = "test";
	args["from"] = "en";
	args["to"] = "zh";
	args["transtype"] = "translang";
	args["simple_means_flag"] = "3";
	requesterPost->addHeader("X-Requested-With","XMLHttpRequest");
	requesterPost->addHeader("Origin","http://fanyi.baidu.com");

	requesterPost->setBody(args.make());
	requesterPost->startRequester("http://fanyi.baidu.com/langdetect", [](const SockException &ex,
					const string &status,
					const HttpClient::HttpHeader &header,
					const string &strRecvBody){
		if(ex){
			WarnL << "network err:" << ex.getErrCode() << " " << ex.what();
		} else {
			_StrPrinter printer;
			for(auto &pr: header){
				printer << pr.first << ":" << pr.second << "\r\n";
			}
			InfoL << "\r\nhttp status:" << status << "\r\n\r\n"
					<< "header:" << (printer << endl)
					<< "\r\nbody:" << strRecvBody;
		}
	});

	EventPoller::Instance().runLoop();
	static onceToken token(nullptr,[](){
		EventPoller::Destory();
		Logger::Destory();
	});
	return 0;
}

