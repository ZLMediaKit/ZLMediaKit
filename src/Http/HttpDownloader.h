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

#ifndef SRC_HTTP_HTTPDOWNLOADER_H_
#define SRC_HTTP_HTTPDOWNLOADER_H_

#include "HttpClientImp.h"

namespace ZL {
namespace Http {

class HttpDownloader: public HttpClientImp {
public:
	typedef std::shared_ptr<HttpDownloader> Ptr;
	typedef std::function<void(ErrCode code,const char *errMsg,const char *filePath)> onDownloadResult;
	HttpDownloader();
	virtual ~HttpDownloader();
	//开始下载文件,默认断点续传方式下载
	void startDownload(const string &url,const string &filePath = "",bool bAppend = false, float timeOutSecond = 10 );
	void startDownload(const string &url,const onDownloadResult &cb,float timeOutSecond = 10){
		setOnResult(cb);
		startDownload(url,"",false,timeOutSecond);
	}
	void setOnResult(const onDownloadResult &cb){
		_onResult = cb;
	}
private:
	void onResponseHeader(const string &status,const HttpHeader &headers) override;
	void onResponseBody(const char *buf,size_t size,size_t recvedSize,size_t totalSize) override;
	void onResponseCompleted() override;
	void onDisconnect(const SockException &ex) override;
    void onManager() override;

    void closeFile();

	FILE *_saveFile = nullptr;
	string _filePath;
	onDownloadResult _onResult;
    uint32_t _timeOutSecond;
	bool _bDownloadSuccess = false;
    Ticker _downloadTicker;
};

} /* namespace Http */
} /* namespace ZL */

#endif /* SRC_HTTP_HTTPDOWNLOADER_H_ */
