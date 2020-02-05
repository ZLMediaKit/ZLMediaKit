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

#include "Recorder.h"
#include "Common/config.h"
#include "Common/MediaSource.h"
#include "MP4Recorder.h"
#include "HlsRecorder.h"

using namespace toolkit;

namespace mediakit {

string Recorder::getRecordPath(Recorder::type type, const string &vhost, const string &app, const string &stream_id, const string &customized_path) {
    GET_CONFIG(bool, enableVhost, General::kEnableVhost);
    switch (type) {
        case Recorder::type_hls: {
            GET_CONFIG(string, hlsPath, Hls::kFilePath);
            string m3u8FilePath;
            if (enableVhost) {
                m3u8FilePath = vhost + "/" + app + "/" + stream_id + "/hls.m3u8";
            } else {
                m3u8FilePath = app + "/" + stream_id + "/hls.m3u8";
            }
            //Here we use the customized file path.
            if (!customized_path.empty()) {
                m3u8FilePath = customized_path + "/hls.m3u8";
            }
            return File::absolutePath(m3u8FilePath, hlsPath);
        }
        case Recorder::type_mp4: {
            GET_CONFIG(string, recordPath, Record::kFilePath);
            GET_CONFIG(string, recordAppName, Record::kAppName);
            string mp4FilePath;
            if (enableVhost) {
                mp4FilePath = vhost + "/" + recordAppName + "/" + app + "/" + stream_id + "/";
            } else {
                mp4FilePath = recordAppName + "/" + app + "/" + stream_id + "/";
            }
            //Here we use the customized file path.
            if (!customized_path.empty()) {
                mp4FilePath = customized_path + "/";
            }
            return File::absolutePath(mp4FilePath, recordPath);
        }
        default:
            return "";
    }
}
////////////////////////////////////////////////////////////////////////////////////////

class RecorderHelper {
public:
    typedef std::shared_ptr<RecorderHelper> Ptr;

    /**
     * 构建函数
     * @param bContinueRecord false表明hls录制从头开始录制(意味着hls临时文件在媒体反注册时会被删除)
     */
    RecorderHelper(const MediaSinkInterface::Ptr &recorder, bool bContinueRecord) {
        _recorder = recorder;
        _continueRecord = bContinueRecord;
    }

    ~RecorderHelper() {
        resetTracks();
    }

    // 附则于track上
    void attachTracks(vector<Track::Ptr> &&tracks, const string &schema){
        if(isTracksSame(tracks)){
            return;
        }
        resetTracks();
        _tracks = std::move(tracks);
        _schema = schema;
        for (auto &track : _tracks) {
            _recorder->addTrack(track);
            track->addDelegate(_recorder);
        }
    }


    // 判断新的tracks是否与之前的一致
    bool isTracksSame(const vector<Track::Ptr> &tracks){
        if(tracks.size() != _tracks.size()) {
            return false;
        }
        int i = 0;
        for(auto &track : tracks){
            if(track != _tracks[i++]){
                return false;
            }
        }
        return true;
    }

    // 重置所有track
    void resetTracks(){
        if(_tracks.empty()){
            return;
        }
        for (auto &track : _tracks) {
            track->delDelegate(_recorder.get());
        }
        _tracks.clear();
        _recorder->resetTracks();
    }

    // 返回false表明hls录制从头开始录制(意味着hls临时文件在媒体反注册时会被删除)
    bool continueRecord(){
        return _continueRecord;
    }

    bool isRecording() {
        return !_tracks.empty();
    }

    const string &getSchema() const{
        return _schema;
    }

    const MediaSinkInterface::Ptr& getRecorder() const{
        return _recorder;
    }
private:
    MediaSinkInterface::Ptr _recorder;
    vector<Track::Ptr> _tracks;
    bool _continueRecord;
    string _schema;
};


template<Recorder::type type>
class MediaSourceWatcher {
public:
    static MediaSourceWatcher& Instance(){
        static MediaSourceWatcher instance;
        return instance;
    }

    Recorder::status getRecordStatus(const string &vhost, const string &app, const string &stream_id) {
        return getRecordStatus_l(getRecorderKey(vhost, app, stream_id));
    }

    MediaSinkInterface::Ptr getRecorder(const string &vhost, const string &app, const string &stream_id) const{
        auto key = getRecorderKey(vhost, app, stream_id);
        lock_guard<decltype(_recorder_mtx)> lck(_recorder_mtx);
        auto it = _recorder_map.find(key);
        if (it == _recorder_map.end()) {
            return nullptr;
        }
        return it->second->getRecorder();
    }

    int startRecord(const string &vhost, const string &app, const string &stream_id, const string &customized_path, bool waitForRecord, bool continueRecord) {
        auto key = getRecorderKey(vhost, app, stream_id);
        lock_guard<decltype(_recorder_mtx)> lck(_recorder_mtx);
        if (getRecordStatus_l(key) != Recorder::status_not_record) {
            // 已经在录制了
            return 0;
        }

        auto src = findMediaSource(vhost, app, stream_id);
        if (!waitForRecord && !src) {
            // 暂时无法开启录制
            return -1;
        }

        auto recorder = Recorder::createRecorder(type, vhost, app, stream_id, customized_path);
        if (!recorder) {
            // 创建录制器失败
            WarnL << "不支持该录制类型:" << type;
            return -2;
        }
        auto helper = std::make_shared<RecorderHelper>(recorder, continueRecord);
        if(src){
            auto tracks = src->getTracks(needTrackReady());
            if(tracks.size()){
                helper->attachTracks(std::move(tracks),src->getSchema());
            }
            auto hls_recorder = dynamic_pointer_cast<HlsRecorder>(recorder);
            if(hls_recorder){
                hls_recorder->getMediaSource()->setListener(src->getListener());
            }
        }

        _recorder_map[key] = std::move(helper);
        return 0;
    }

    bool stopRecord(const string &vhost, const string &app, const string &stream_id) {
        lock_guard<decltype(_recorder_mtx)> lck(_recorder_mtx);
        return _recorder_map.erase(getRecorderKey(vhost, app, stream_id));
    }

    void stopAll(){
        lock_guard<decltype(_recorder_mtx)> lck(_recorder_mtx);
        _recorder_map.clear();
    }

private:
    MediaSourceWatcher(){
        //保存NoticeCenter的强引用，防止在MediaSourceWatcher单例释放前释放NoticeCenter单例
        _notice_center = NoticeCenter::Instance().shared_from_this();
        _notice_center->addListener(this,Broadcast::kBroadcastMediaChanged,[this](BroadcastMediaChangedArgs){
            if(!bRegist){
                removeRecorder(sender);
            }
        });
        _notice_center->addListener(this,Broadcast::kBroadcastMediaResetTracks,[this](BroadcastMediaResetTracksArgs){
            addRecorder(sender);
        });
    }

    ~MediaSourceWatcher(){
        _notice_center->delListener(this,Broadcast::kBroadcastMediaChanged);
        _notice_center->delListener(this,Broadcast::kBroadcastMediaResetTracks);
    }

    void addRecorder(MediaSource &sender){
        auto tracks = sender.getTracks(needTrackReady());
        auto key = getRecorderKey(sender.getVhost(),sender.getApp(),sender.getId());
        lock_guard<decltype(_recorder_mtx)> lck(_recorder_mtx);
        auto it = _recorder_map.find(key);
        if(it == _recorder_map.end()){
            // 录像记录不存在
            return;
        }

        if(!it->second->isRecording() || it->second->getSchema() == sender.getSchema()){
            // 绑定的协议一致或者并未正在录制则替换tracks
            if (!tracks.empty()) {
                it->second->attachTracks(std::move(tracks),sender.getSchema());
            }
        }
    }

    void removeRecorder(MediaSource &sender){
        auto key = getRecorderKey(sender.getVhost(),sender.getApp(),sender.getId());
        lock_guard<decltype(_recorder_mtx)> lck(_recorder_mtx);
        auto it = _recorder_map.find(key);
        if(it == _recorder_map.end() || it->second->getSchema() != sender.getSchema()){
            // 录像记录不存在或绑定的协议不一致
            return;
        }

        if(it->second->continueRecord()){
            // 如果可以继续录制，那么只重置tracks,不删除对象
            it->second->resetTracks();
        }else{
            // 删除对象(意味着可能删除hls临时文件)
            _recorder_map.erase(it);
        }
    }

    Recorder::status getRecordStatus_l(const string &key) {
        auto it = _recorder_map.find(key);
        if (it == _recorder_map.end()) {
            return Recorder::status_not_record;
        }
        return it->second->isRecording() ? Recorder::status_recording : Recorder::status_wait_record;
    }

    // 查找MediaSource以便录制
    MediaSource::Ptr findMediaSource(const string &vhost, const string &app, const string &stream_id) {
        bool need_ready = needTrackReady();
        auto src = MediaSource::find(RTMP_SCHEMA, vhost, app, stream_id);
        if (src) {
            auto ret = src->getTracks(need_ready);
            if (!ret.empty()) {
                return std::move(src);
            }
        }

        src = MediaSource::find(RTSP_SCHEMA, vhost, app, stream_id);
        if (src) {
            auto ret = src->getTracks(need_ready);
            if (!ret.empty()) {
                return std::move(src);
            }
        }
        return nullptr;
    }

    string getRecorderKey(const string &vhost, const string &app, const string &stream_id) const{
        return vhost + "/" + app + "/" + stream_id;
    }


    /**
     * 有些录制类型不需要track就绪即可录制
     */
    bool needTrackReady(){
        switch (type){
            case Recorder::type_hls:
                return false;
            case Recorder::type_mp4:
                return true;
            default:
                return true;
        }
    }
private:
    mutable recursive_mutex _recorder_mtx;
    NoticeCenter::Ptr _notice_center;
    unordered_map<string, RecorderHelper::Ptr> _recorder_map;
};


Recorder::status Recorder::getRecordStatus(Recorder::type type, const string &vhost, const string &app, const string &stream_id) {
    switch (type){
        case type_mp4:
            return MediaSourceWatcher<type_mp4>::Instance().getRecordStatus(vhost,app,stream_id);
        case type_hls:
            return MediaSourceWatcher<type_hls>::Instance().getRecordStatus(vhost,app,stream_id);
    }
    return status_not_record;
}

std::shared_ptr<MediaSinkInterface> Recorder::getRecorder(type type, const string &vhost, const string &app, const string &stream_id){
    switch (type){
        case type_mp4:
            return MediaSourceWatcher<type_mp4>::Instance().getRecorder(vhost,app,stream_id);
        case type_hls:
            return MediaSourceWatcher<type_hls>::Instance().getRecorder(vhost,app,stream_id);
    }
    return nullptr;
}

std::shared_ptr<MediaSinkInterface> Recorder::createRecorder(type type, const string &vhost, const string &app, const string &stream_id, const string &customized_path){
    auto path = Recorder::getRecordPath(type, vhost, app, stream_id);
    switch (type) {
        case Recorder::type_hls: {
#if defined(ENABLE_HLS)
            auto ret = std::make_shared<HlsRecorder>(path, string(VHOST_KEY) + "=" + vhost);
            ret->setMediaSource(vhost, app, stream_id);
            return ret;
#endif
            return nullptr;
        }

        case Recorder::type_mp4: {
#if defined(ENABLE_MP4RECORD)
            return std::make_shared<MP4Recorder>(path, vhost, app, stream_id);
#endif
            return nullptr;
        }

        default:
            return nullptr;
    }
}

int Recorder::startRecord(Recorder::type type, const string &vhost, const string &app, const string &stream_id, const string &customized_path, bool waitForRecord, bool continueRecord) {
    switch (type){
        case type_mp4:
            return MediaSourceWatcher<type_mp4>::Instance().startRecord(vhost,app,stream_id,customized_path,waitForRecord,continueRecord);
        case type_hls:
            return MediaSourceWatcher<type_hls>::Instance().startRecord(vhost,app,stream_id,customized_path,waitForRecord,continueRecord);
    }
    WarnL << "unknown record type: " << type;
    return -3;
}

bool Recorder::stopRecord(Recorder::type type, const string &vhost, const string &app, const string &stream_id) {
    switch (type){
        case type_mp4:
            return MediaSourceWatcher<type_mp4>::Instance().stopRecord(vhost,app,stream_id);
        case type_hls:
            return MediaSourceWatcher<type_hls>::Instance().stopRecord(vhost,app,stream_id);
    }
    return false;
}

void Recorder::stopAll() {
    MediaSourceWatcher<type_hls>::Instance().stopAll();
    MediaSourceWatcher<type_mp4>::Instance().stopAll();
}

} /* namespace mediakit */
