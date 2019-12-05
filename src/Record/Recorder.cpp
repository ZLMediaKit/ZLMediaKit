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

MediaSinkInterface *createHlsRecorder(const string &strVhost_tmp, const string &strApp, const string &strId) {
#if defined(ENABLE_HLS)
    GET_CONFIG(bool, enableVhost, General::kEnableVhost);
    GET_CONFIG(string, hlsPath, Hls::kFilePath);

    string strVhost = strVhost_tmp;
    if (trim(strVhost).empty()) {
        //如果strVhost为空，则强制为默认虚拟主机
        strVhost = DEFAULT_VHOST;
    }

    string m3u8FilePath;
    string params;
    if (enableVhost) {
        m3u8FilePath = strVhost + "/" + strApp + "/" + strId + "/hls.m3u8";
        params = string(VHOST_KEY) + "=" + strVhost;
    } else {
        m3u8FilePath = strApp + "/" + strId + "/hls.m3u8";
    }
    m3u8FilePath = File::absolutePath(m3u8FilePath, hlsPath);
    return new HlsRecorder(m3u8FilePath, params);
#else
    return nullptr;
#endif //defined(ENABLE_HLS)
}

MediaSinkInterface *createMP4Recorder(const string &strVhost_tmp, const string &strApp, const string &strId) {
#if defined(ENABLE_MP4RECORD)
    GET_CONFIG(bool, enableVhost, General::kEnableVhost);
    GET_CONFIG(string, recordPath, Record::kFilePath);
    GET_CONFIG(string, recordAppName, Record::kAppName);

    string strVhost = strVhost_tmp;
    if (trim(strVhost).empty()) {
        //如果strVhost为空，则强制为默认虚拟主机
        strVhost = DEFAULT_VHOST;
    }

    string mp4FilePath;
    if (enableVhost) {
        mp4FilePath = strVhost + "/" + recordAppName + "/" + strApp + "/" + strId + "/";
    } else {
        mp4FilePath = recordAppName + "/" + strApp + "/" + strId + "/";
    }
    mp4FilePath = File::absolutePath(mp4FilePath, recordPath);
    return new MP4Recorder(mp4FilePath, strVhost, strApp, strId);
#else
    return nullptr;
#endif //defined(ENABLE_MP4RECORD)
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

    int startRecord(const string &vhost, const string &app, const string &stream_id, bool waitForRecord, bool continueRecord) {
        auto key = getRecorderKey(vhost, app, stream_id);
        lock_guard<decltype(_recorder_mtx)> lck(_recorder_mtx);
        if (getRecordStatus_l(key) != Recorder::status_not_record) {
            // 已经在录制了
            return 0;
        }

        string schema;
        auto tracks = findTracks(vhost, app, stream_id,schema);
        if (!waitForRecord && tracks.empty()) {
            // 暂时无法开启录制
            return -1;
        }

        auto recorder = MediaSinkInterface::Ptr(createRecorder(vhost, app, stream_id));
        if (!recorder) {
            // 创建录制器失败
            return -2;
        }
        auto helper = std::make_shared<RecorderHelper>(recorder, continueRecord);
        if(tracks.size()){
            helper->attachTracks(std::move(tracks),schema);
        }
        _recorder_map[key] = std::move(helper);
        return 0;
    }

    void stopRecord(const string &vhost, const string &app, const string &stream_id) {
        lock_guard<decltype(_recorder_mtx)> lck(_recorder_mtx);
        _recorder_map.erase(getRecorderKey(vhost, app, stream_id));
    }

    void stopAll(){
        lock_guard<decltype(_recorder_mtx)> lck(_recorder_mtx);
        _recorder_map.clear();
    }

private:
    MediaSourceWatcher(){
        NoticeCenter::Instance().addListener(this,Broadcast::kBroadcastMediaChanged,[this](BroadcastMediaChangedArgs){
            if(bRegist){
                onRegist(schema,vhost,app,stream,sender);
            }else{
                onUnRegist(schema,vhost,app,stream,sender);
            }
        });
        NoticeCenter::Instance().addListener(this,Broadcast::kBroadcastMediaResetTracks,[this](BroadcastMediaResetTracksArgs){
            onRegist(schema,vhost,app,stream,sender);
        });
    }

    ~MediaSourceWatcher(){
        NoticeCenter::Instance().delListener(this,Broadcast::kBroadcastMediaChanged);
        NoticeCenter::Instance().delListener(this,Broadcast::kBroadcastMediaResetTracks);
    }

    void onRegist(const string &schema,const string &vhost,const string &app,const string &stream,MediaSource &sender){
        auto key = getRecorderKey(vhost,app,stream);
        lock_guard<decltype(_recorder_mtx)> lck(_recorder_mtx);
        auto it = _recorder_map.find(key);
        if(it == _recorder_map.end()){
            // 录像记录不存在
            return;
        }

        if(!it->second->isRecording() || it->second->getSchema() == schema){
            // 绑定的协议一致或者并未正在录制则替换tracks
            auto tracks = sender.getTracks(true);
            if (!tracks.empty()) {
                it->second->attachTracks(std::move(tracks),schema);
            }
        }
    }

    void onUnRegist(const string &schema,const string &vhost,const string &app,const string &stream,MediaSource &sender){
        auto key = getRecorderKey(vhost,app,stream);
        lock_guard<decltype(_recorder_mtx)> lck(_recorder_mtx);
        auto it = _recorder_map.find(key);
        if(it == _recorder_map.end() || it->second->getSchema() != schema){
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
    vector<Track::Ptr> findTracks(const string &vhost, const string &app, const string &stream_id,string &schema) {
        auto src = MediaSource::find(RTMP_SCHEMA, vhost, app, stream_id);
        if (src) {
            auto ret = src->getTracks(true);
            if (!ret.empty()) {
                schema = RTMP_SCHEMA;
                return std::move(ret);
            }
        }

        src = MediaSource::find(RTSP_SCHEMA, vhost, app, stream_id);
        if (src) {
            schema = RTSP_SCHEMA;
            return src->getTracks(true);
        }
        return vector<Track::Ptr>();
    }

    string getRecorderKey(const string &vhost, const string &app, const string &stream_id) {
        return vhost + "/" + app + "/" + stream_id;
    }

    MediaSinkInterface *createRecorder(const string &vhost, const string &app, const string &stream_id) {
        MediaSinkInterface *ret = nullptr;
        switch (type) {
            case Recorder::type_hls:
                ret = createHlsRecorder(vhost, app, stream_id);
                break;
            case Recorder::type_mp4:
                ret = createMP4Recorder(vhost, app, stream_id);
                break;
            default:
                break;
        }
        if(!ret){
            WarnL << "can not create recorder of type: " << type;
        }
        return ret;
    }
private:
    recursive_mutex _recorder_mtx;
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

int Recorder::startRecord(Recorder::type type, const string &vhost, const string &app, const string &stream_id, bool waitForRecord, bool continueRecord) {
    switch (type){
        case type_mp4:
            return MediaSourceWatcher<type_mp4>::Instance().startRecord(vhost,app,stream_id,waitForRecord,continueRecord);
        case type_hls:
            return MediaSourceWatcher<type_hls>::Instance().startRecord(vhost,app,stream_id,waitForRecord,continueRecord);
    }
    WarnL << "unknown record type: " << type;
    return -3;
}

void Recorder::stopRecord(Recorder::type type, const string &vhost, const string &app, const string &stream_id) {
    switch (type){
        case type_mp4:
            return MediaSourceWatcher<type_mp4>::Instance().stopRecord(vhost,app,stream_id);
        case type_hls:
            return MediaSourceWatcher<type_hls>::Instance().stopRecord(vhost,app,stream_id);
    }
}

void Recorder::stopAll() {
    MediaSourceWatcher<type_hls>::Instance().stopAll();
    MediaSourceWatcher<type_mp4>::Instance().stopAll();
}

} /* namespace mediakit */
