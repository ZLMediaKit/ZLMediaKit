/*
 * HttpDownloader.h
 *
 *  Created on: 2017年5月5日
 *      Author: xzl
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
	void startDownload(const string &url,const string &filePath = "",bool bAppend = false);
	void startDownload(const string &url,const onDownloadResult &cb){
		setOnResult(cb);
		startDownload(url);
	}
	void setOnResult(const onDownloadResult &cb){
		_onResult = cb;
	}
private:
	void onResponseHeader(const string &status,const HttpHeader &headers) override;
	void onResponseBody(const char *buf,size_t size,size_t recvedSize,size_t totalSize) override;
	void onResponseCompleted() override;
	void onDisconnect(const SockException &ex) override;
	void closeFile();

	FILE *_saveFile = nullptr;
	string _filePath;
	onDownloadResult _onResult;
	bool _bDownloadSuccess = false;
};

} /* namespace Http */
} /* namespace ZL */

#endif /* SRC_HTTP_HTTPDOWNLOADER_H_ */
