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

void HttpDownloader::startDownload(const string& url, const string& filePath,bool bAppend,float timeOutSecond) {
	_filePath = filePath;
    _timeOutSecond = timeOutSecond;
    _downloadTicker.resetTime();
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
	sendRequest(url,timeOutSecond);
}

void HttpDownloader::onResponseHeader(const string& status,const HttpHeader& headers) {
    _downloadTicker.resetTime();
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
    _downloadTicker.resetTime();
    if(_saveFile){
		fwrite(buf,size,1,_saveFile);
	}
}
//string getMd5Sum(const string &filePath){
//	auto fp = File::createfile_file(filePath.data(),"rb");
//	fseek(fp,0,SEEK_END);
//	auto sz = ftell(fp);
//	char tmp[sz];
//	fseek(fp,0,SEEK_SET);
//	auto rd = fread(tmp,1,sz,fp);
//	InfoL << sz << " " << rd;
//	fclose(fp);
//	return MD5(string(tmp,sz)).hexdigest();
//}
void HttpDownloader::onResponseCompleted() {
	closeFile();
	//InfoL << "md5Sum:" << getMd5Sum(_filePath);
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
		fflush(_saveFile);
		fclose(_saveFile);
		_saveFile = nullptr;
	}
}

void HttpDownloader::onManager(){
    if(_downloadTicker.elapsedTime() > _timeOutSecond * 1000){
        //超时
        onDisconnect(SockException(Err_timeout,"download timeout"));
        shutdown();
    }
}


} /* namespace Http */
} /* namespace ZL */
