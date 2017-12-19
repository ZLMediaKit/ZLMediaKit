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

#include "httpdownloader.h"

#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Util/onceToken.h"
#include "Http/HttpDownloader.h"
#include "cleaner.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Http;


static recursive_mutex s_mtxMapDownloader;
static unordered_map<void *, HttpDownloader::Ptr> s_mapDownloader;

static onceToken s_token([](){
	cleaner::Instance().push_front([](){
		lock_guard<recursive_mutex> lck(s_mtxMapDownloader);
		s_mapDownloader.clear();
		DebugL << "clear httpdownloader" << endl;
	});
},nullptr);



API_EXPORT HttpDownloaderContex API_CALL createDownloader(){
	HttpDownloader::Ptr ret(new HttpDownloader());
	lock_guard<recursive_mutex> lck(s_mtxMapDownloader);
	s_mapDownloader.emplace(ret.get(),ret);
	return ret.get();
}
API_EXPORT void API_CALL downloader_startDownload(HttpDownloaderContex ctx,const char *url,downloader_onResult cb,void *userData){
	HttpDownloader *ptr = (HttpDownloader *)ctx;
	string urlTmp(url);
	ptr->startDownload(url, [cb,userData,urlTmp](int code,const char *errMsg,const char *filePath){
		if(cb){
			InfoL << code << " " << errMsg << " " << filePath << " " << urlTmp;
			cb(userData,code,errMsg,filePath);
		}
	});
}
API_EXPORT void API_CALL releaseDownloader(HttpDownloaderContex ctx){
	lock_guard<recursive_mutex> lck(s_mtxMapDownloader);
	s_mapDownloader.erase(ctx);
}

