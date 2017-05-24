/*
 * HttpDownloader.cpp
 *
 *  Created on: 2017年5月5日
 *      Author: xzl
 */

#include "HttpDownloader.h"
#include "Util/MD5.h"
#include "Util/File.h"

using namespace ZL::Util;

namespace ZL {
namespace Http {

HttpDownloader::HttpDownloader() {

}

HttpDownloader::~HttpDownloader() {
	closeFile();
}

void HttpDownloader::startDownload(const string& url, const string& filePath,bool bAppend) {
	_filePath = filePath;
	if(_filePath.empty()){
		_filePath = exeDir() + "HttpDownloader/" + MD5(url).hexdigest();
	}
	_saveFile = File::createfile_file(_filePath.data(),bAppend ? "ab" : "wb");
	if(!_saveFile){
		auto strErr = StrPrinter << "打开文件失败:" << filePath << endl;
		throw std::runtime_error(strErr);
	}
	_bDownloadSuccess = false;
	if(bAppend){
		auto currentLen = ftell(_saveFile);
		if(currentLen){
			//最少续传一个字节，怕遇到http 416的错误
			currentLen -= 1;
			fseek(_saveFile,-1,SEEK_CUR);
		}
		addHeader("Range", StrPrinter << "bytes=" << currentLen << "-" << endl);
	}
	setMethod("GET");
	sendRequest(url);
}

void HttpDownloader::onResponseHeader(const string& status,const HttpHeader& headers) {
	if(status != "200" && status != "206"){
		//失败
		shutdown();
		closeFile();
		File::delete_file(_filePath.data());
		if(_onResult){
			auto errMsg = StrPrinter << "Http Status:" << status << endl;
			_onResult(Err_other,errMsg.data(),_filePath.data());
			_onResult = nullptr;
		}
	}
}

void HttpDownloader::onResponseBody(const char* buf, size_t size, size_t recvedSize, size_t totalSize) {
	if(_saveFile){
		fwrite(buf,size,1,_saveFile);
	}
}

void HttpDownloader::onResponseCompleted() {
	closeFile();
	_bDownloadSuccess = true;
	if(_onResult){
		_onResult(Err_success,"success",_filePath.data());
		_onResult = nullptr;
	}
}

void HttpDownloader::onDisconnect(const SockException &ex) {
	closeFile();
	if(!_bDownloadSuccess){
		File::delete_file(_filePath.data());
	}
	if(_onResult){
		_onResult(ex.getErrCode(),ex.what(),_filePath.data());
		_onResult = nullptr;
	}
}

void HttpDownloader::closeFile() {
	if(_saveFile){
		fclose(_saveFile);
		_saveFile = nullptr;
	}
}

} /* namespace Http */
} /* namespace ZL */
