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

#include <algorithm>
#include "MediaPusher.h"
#include "PusherBase.h"

using namespace toolkit;

namespace mediakit {

MediaPusher::MediaPusher(const MediaSource::Ptr &src) {
    _src = src;
}

MediaPusher::MediaPusher(const string &schema,
                         const string &strVhost,
                         const string &strApp,
                         const string &strStream) {
    _src = MediaSource::find(schema,strVhost,strApp,strStream);
}

MediaPusher::~MediaPusher() {
}
void MediaPusher::publish(const string &strUrl) {
    _parser = PusherBase::createPusher(_src,strUrl);
    _parser->setOnShutdown(_shutdownCB);
    _parser->setOnPublished(_publishCB);
    _parser->mINI::operator=(*this);
    _parser->publish(strUrl);
}

EventPoller::Ptr MediaPusher::getPoller(){
    auto parser = dynamic_pointer_cast<SocketHelper>(_parser);
    if(!parser){
        return nullptr;
    }
    return parser->getPoller();
}



} /* namespace mediakit */
