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

#include "HttpClient.h"
#include "Rtsp/Rtsp.h"

namespace ZL {
namespace Http {


HttpClient::HttpClient(){
}
HttpClient::~HttpClient(){
}
void HttpClient::sendRequest(const string &strUrl,float fTimeOutSec){
    auto protocol = FindField(strUrl.data(), NULL , "://");
    uint16_t defaultPort;
    bool isHttps;
    if (strcasecmp(protocol.data(), "http") == 0) {
        defaultPort = 80;
        isHttps = false;
    }else if(strcasecmp(protocol.data(), "https") ==0 ){
        defaultPort = 443;
        isHttps = true;
    }else{
        auto strErr = StrPrinter << "非法的协议:" << protocol << endl;
        throw std::invalid_argument(strErr);
    }
    
    auto host = FindField(strUrl.data(), "://", "/");
    if (host.empty()) {
        host = FindField(strUrl.data(), "://", NULL);
    }
    _path = FindField(strUrl.data(), host.data(), NULL);
    if (_path.empty()) {
        _path = "/";
    }
    auto port = atoi(FindField(host.data(), ":", NULL).data());
    if (port <= 0) {
        //默认端口
        port = defaultPort;
    } else {
        //服务器域名
        host = FindField(host.data(), NULL, ":");
    }
    _header.emplace(string("Host"),host);
    _header.emplace(string("Tools"),"ZLMediaKit");
    _header.emplace(string("Connection"),"keep-alive");
    _header.emplace(string("Accept"),"*/*");
    _header.emplace(string("Accept-Language"),"zh-CN,zh;q=0.8");
    _header.emplace(string("User-Agent"),"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_12_1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/57.0.2987.133 Safari/537.36");
    
    if(!_body.empty()){
        _header.emplace(string("Content-Length"),to_string(_body.size()));
        _header.emplace(string("Content-Type"),"application/x-www-form-urlencoded; charset=UTF-8");
    }

    bool bChanged = (_lastHost != host + ":" + to_string(port)) || (_isHttps != isHttps);
    _lastHost = host + ":" + to_string(port);
    _isHttps = isHttps;

    if(!alive() || bChanged){
        //InfoL << "reconnet:" << _lastHost;
        startConnect(host, port,fTimeOutSec);
    }else{
        SockException ex;
        onConnect(ex);
    }
}


void  HttpClient::onConnect(const SockException &ex) {
	if(ex){
		onDisconnect(ex);
		return;
	}
    _recvedBodySize = -1;
    _recvedResponse.clear();
    send(_method + " ");
    send(_path + " HTTP/1.1\r\n");
    for (auto &pr : _header) {
        send(pr.first + ": ");
        send(pr.second + "\r\n");
    }
    send("\r\n");
    if (!_body.empty()) {
        send(_body);
    }
}
void  HttpClient::onRecv(const Buffer::Ptr &pBuf) {
	onRecvBytes(pBuf->data(),pBuf->size());
}

void  HttpClient::onErr(const SockException &ex) {
	onDisconnect(ex);
}

void HttpClient::onRecvBytes(const char* data, int size) {
	if(_recvedBodySize == -1){
		//还没有收到http body，这只是http头
		auto lastLen = _recvedResponse.size();
		_recvedResponse.append(data,size);
		auto pos = _recvedResponse.find("\r\n\r\n",lastLen);
		if(pos == string::npos){
			//http 头还未收到
			return;
		}
		_parser.Parse(_recvedResponse.data());
		onResponseHeader(_parser.Url(),_parser.getValues());

		_totalBodySize = atoll(((HttpHeader &)_parser.getValues())["Content-Length"].data());
		_recvedBodySize = _recvedResponse.size() - pos - 4;
		if(_totalBodySize < _recvedBodySize){
			//http body 比声明的大 这个不可能的
			_StrPrinter printer;
			for(auto &pr: _parser.getValues()){
				printer << pr.first << ":" << pr.second << "\r\n";
			}
			FatalL << _totalBodySize << ":" << _recvedBodySize << "\r\n" << (printer << endl);
			shutdown();
			return;
		}
		if (_recvedBodySize) {
			//_recvedResponse里面包含body负载
			onResponseBody(_recvedResponse.data() + _recvedResponse.size() - _recvedBodySize, _recvedBodySize,_recvedBodySize,_totalBodySize);
		}

		if(_recvedBodySize >= _totalBodySize){
			onResponseCompleted();
		}
		_recvedResponse.clear();
		return;
	}
	//http body
	if(_recvedBodySize < _totalBodySize){
		_recvedBodySize += size;
		onResponseBody(data,size,_recvedBodySize,_totalBodySize);
		if(_recvedBodySize >= _totalBodySize){
			onResponseCompleted();
		}
		return;
	}
	//http body 比声明的大 这个不可能的
	_StrPrinter printer;
	for(auto &pr: _parser.getValues()){
		printer << pr.first << ":" << pr.second << "\r\n";
	}
	FatalL << _totalBodySize << ":" << _recvedBodySize << "\r\n" << (printer << endl);
	shutdown();
}
    
    
} /* namespace Http */
} /* namespace ZL */

