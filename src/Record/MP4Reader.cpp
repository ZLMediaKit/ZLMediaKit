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

#ifdef ENABLE_MP4
#include "MP4Reader.h"
#include "Common/config.h"
#include "Thread/WorkThreadPool.h"
using namespace toolkit;
namespace mediakit {

MP4Reader::MP4Reader(const string &strVhost,const string &strApp, const string &strId,const string &filePath ) {
    _poller = WorkThreadPool::Instance().getPoller();
    auto strFileName = filePath;
    if(strFileName.empty()){
        GET_CONFIG(string,recordPath,Record::kFilePath);
        GET_CONFIG(bool,enableVhost,General::kEnableVhost);
        if(enableVhost){
            strFileName = strVhost + "/" + strApp + "/" + strId;
        }else{
            strFileName = strApp + "/" + strId;
        }
        strFileName = File::absolutePath(strFileName,recordPath);
    }

    _demuxer = std::make_shared<MP4Demuxer>(strFileName.data());
    _mediaMuxer.reset(new MultiMediaSourceMuxer(strVhost, strApp, strId, _demuxer->getDurationMS() / 1000.0, true, true, false, false));
    for(auto &track : _demuxer->getTracks(false)){
        _mediaMuxer->addTrack(track);
        if(track->getTrackType() == TrackVideo){
            _have_video = true;
        }
    }
    //添加完毕所有track，防止单track情况下最大等待3秒
    _mediaMuxer->addTrackCompleted();
}

bool MP4Reader::readSample() {
    bool eof = false;
    while (!eof) {
        auto frame = _demuxer->readFrame(false, &eof);
        if (!frame) {
            break;
        }
        _mediaMuxer->inputFrame(frame);
        if (frame->dts() > nextStampForStop()) {
            break;
        }
    }

    if(!eof && _mediaMuxer->totalReaderCount() > 0){
        //文件未看完且观看者大于0个
        _alive.resetTime();
    }

    //重头开始循环读取
    GET_CONFIG(bool,fileRepeat,Record::kFileRepeat);
    if(eof && fileRepeat){
        //文件看完了，且需要从头开始看
        seekTo(0);
    }
    //读取mp4完毕后10秒才销毁对象
    return _alive.elapsedTime() < 10 * 1000;
}

void MP4Reader::startReadMP4() {
    GET_CONFIG(uint32_t, sampleMS, Record::kSampleMS);
    auto strongSelf = shared_from_this();
    _mediaMuxer->setListener(strongSelf);

    //先获取关键帧
    seekTo(0);
    //设置下次读取停止事件
    setNextStampForStop(_seek_to + sampleMS);
    //读sampleMS毫秒的数据用于产生MediaSource
    readSample();

    //启动定时器
    _timer = std::make_shared<Timer>(sampleMS / 1000.0f, [strongSelf]() {
        lock_guard<recursive_mutex> lck(strongSelf->_mtx);
        return strongSelf->readSample();
    }, _poller);
}

uint32_t MP4Reader::nextStampForStop() {
    return _seek_to + _seek_ticker.elapsedTime();
}

void MP4Reader::setNextStampForStop(uint32_t ui32Stamp){
    _seek_to = ui32Stamp;
    _seek_ticker.resetTime();
    _alive.resetTime();
}

bool MP4Reader::seekTo(MediaSource &sender,uint32_t ui32Stamp){
    return seekTo(ui32Stamp);
}

bool MP4Reader::seekTo(uint32_t ui32Stamp){
    lock_guard<recursive_mutex> lck(_mtx);
    if (ui32Stamp > _demuxer->getDurationMS()) {
        //超过文件长度
        return false;
    }
    auto stamp = _demuxer->seekTo(ui32Stamp);
    if(stamp == -1){
        //seek失败
        return false;
    }
    InfoL << stamp;
    //设置当前时间戳
    setNextStampForStop(stamp);

    if(!_have_video){
        //没有视频，不需要搜索关键帧
        return true;
    }
    //搜索到下一帧关键帧
    bool eof = false;
    while (!eof) {
        auto frame = _demuxer->readFrame(false, &eof);
        if(!frame){
            break;
        }
        if(frame->keyFrame() || frame->configFrame()){
            //定位到key帧
            _mediaMuxer->inputFrame(frame);
            setNextStampForStop(frame->dts());
            return true;
        }
    }
    return false;
}

bool MP4Reader::close(MediaSource &sender,bool force){
    if(!_mediaMuxer || (!force && _mediaMuxer->totalReaderCount())){
        return false;
    }
    _timer.reset();
    WarnL << sender.getSchema() << "/" << sender.getVhost() << "/" << sender.getApp() << "/" << sender.getId() << " " << force;
    return true;
}

int MP4Reader::totalReaderCount(MediaSource &sender) {
    return _mediaMuxer ? _mediaMuxer->totalReaderCount() : sender.readerCount();
}

} /* namespace mediakit */
#endif //ENABLE_MP4


namespace mediakit {
MediaSource::Ptr onMakeMediaSource(const string &strSchema,
                                   const string &strVhost,
                                   const string &strApp,
                                   const string &strId,
                                   const string &filePath,
                                   bool checkApp) {
#ifdef ENABLE_MP4
    GET_CONFIG(string, appName, Record::kAppName);
    if (checkApp && strApp != appName) {
        return nullptr;
    }
    try {
        MP4Reader::Ptr pReader(new MP4Reader(strVhost, strApp, strId, filePath));
        pReader->startReadMP4();
        return MediaSource::find(strSchema, strVhost, strApp, strId, false);
    } catch (std::exception &ex) {
        WarnL << ex.what();
        return nullptr;
    }
#else
    return nullptr;
#endif //ENABLE_MP4
}
}//namespace mediakit
