/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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
using namespace toolkit;

namespace mediakit {

HttpDownloader::HttpDownloader() {

}

HttpDownloader::~HttpDownloader() {
	closeFile();
}

void HttpDownloader::startDownload(const string& url, const string& filePath,bool bAppend,float timeOutSecond) {
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
	sendRequest(url,timeOutSecond);
}

int64_t HttpDownloader::onResponseHeader(const string& status,const HttpHeader& headers) {
    if(status != "200" && status != "206"){
		//失败
		shutdown(SockException(Err_shutdown,StrPrinter << "Http Status:" << status));
	}
	//后续全部是content
	return -1;
}

void HttpDownloader::onResponseBody(const char* buf, int64_t size, int64_t recvedSize, int64_t totalSize) {
    if(_saveFile){
		fwrite(buf,size,1,_saveFile);
	}
}

void HttpDownloader::onResponseCompleted() {
	closeFile();
	//InfoL << "md5Sum:" << getMd5Sum(_filePath);
	_bDownloadSuccess = true;
	if(_onResult){
		_onResult(Err_success,"success",_filePath);
		_onResult = nullptr;
	}
}

void HttpDownloader::onDisconnect(const SockException &ex) {
	closeFile();
	if(!_bDownloadSuccess){
		File::delete_file(_filePath.data());
	}
	if(_onResult){
		_onResult(ex.getErrCode(),ex.what(),_filePath);
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


} /* namespace mediakit */
