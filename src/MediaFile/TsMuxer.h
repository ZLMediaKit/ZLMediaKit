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

#ifndef TSMUXER_H
#define TSMUXER_H

#include <unordered_map>
#include "mpeg-ts.h"
#include "Extension/Frame.h"
#include "Extension/Track.h"
#include "Util/File.h"
using namespace toolkit;

namespace mediakit {

class TsMuxer {
public:
    TsMuxer();
    virtual ~TsMuxer();
    void addTrack(const Track::Ptr &track);
    void inputFrame(const Frame::Ptr &frame);
protected:
    virtual void onTs(const void *packet, int bytes,uint32_t timestamp,int flags) = 0;
    void resetTracks();
private:
    void init();
    void uninit();
private:
    void  *_context = nullptr;
    char *_tsbuf[188];
    uint32_t _timestamp = 0;
    unordered_map<int,int > _codecid_to_stream_id;
    List<Frame::Ptr> _frameCached;
};



class TsMuxer2{
public:
    typedef function<void(const void *packet, int bytes)> onTsCallback;
    TsMuxer2(){
        init();
    }
    ~TsMuxer2(){
        uninit();
    }

    bool addTrack(int track,int codec_id){
        lock_guard<recursive_mutex> lck(_mtx);
        auto it = _allTrackMap.find(track);
        if(it != _allTrackMap.end()){
//            WarnL << "Track:" << track << "已经存在!";
            return false;
        }
        _allTrackMap[track] = codec_id;
        resetAllTracks();
        return true;
    }

    bool removeTrack(int track){
        lock_guard<recursive_mutex> lck(_mtx);
        auto it = _allTrackMap.find(track);
        if(it == _allTrackMap.end()){
//            WarnL << "Track:" << track << "不存在!";
            return false;
        }
        DebugL << "删除Track:" << track;
        _allTrackMap.erase(it);
        resetAllTracks();
        return true;
    }

    bool inputTrackData(int track, const char *data, int length, int64_t pts, int64_t dts, int flags){
        lock_guard<recursive_mutex> lck(_mtx);
        auto it = _track_id_to_stream_id.find(track);
        if(it == _track_id_to_stream_id.end()){
            WarnL << "Track:" << track << "不存在!";
            return false;
        }
        mpeg_ts_write(_context,it->second,flags,pts,dts,data,length);
        return true;
    }

    void setOnTsCallback(const onTsCallback &cb) {
        lock_guard<recursive_mutex> lck(_mtx);
        _onts = cb;
    }

    bool saveToFile(const string &file){
        lock_guard<recursive_mutex> lck(_mtx);
        FILE *fp = File::createfile_file(file.data(),"ab");
        if(!fp){
            WarnL << "打开文件失败:" << file << " " << get_uv_errmsg();
            return false;
        }
        setvbuf(fp, _file_buf, _IOFBF, sizeof(_file_buf));
        _file.reset(fp,[](FILE *fp){
            fclose(fp);
        });
        return true;
    }
private:
    void init() {
        lock_guard<recursive_mutex> lck(_mtx);
        static mpeg_ts_func_t s_func= {
                [](void* param, size_t bytes){
                    TsMuxer2 *muxer = (TsMuxer2 *)param;
                    assert(sizeof(TsMuxer2::_tsbuf) >= bytes);
                    return (void *)muxer->_tsbuf;
                },
                [](void* param, void* packet){
                    //do nothing
                },
                [](void* param, const void* packet, size_t bytes){
                    TsMuxer2 *muxer = (TsMuxer2 *)param;
                    muxer->onTs(packet, bytes);
                }
        };
        if(_context == nullptr){
            _context = mpeg_ts_create(&s_func,this);
        }
    }

    void uninit() {
        lock_guard<recursive_mutex> lck(_mtx);
        if(_context){
            mpeg_ts_destroy(_context);
            _context = nullptr;
        }
        _track_id_to_stream_id.clear();
    }

    void resetAllTracks(){
        lock_guard<recursive_mutex> lck(_mtx);
        uninit();
        init();

        //添加Track
        for (auto &pr : _allTrackMap){
            InfoL << "添加Track:" << pr.first << " " << pr.second;
            _track_id_to_stream_id[pr.first] = mpeg_ts_add_stream(_context,pr.second, nullptr,0);
        }
    }

    void onTs(const void *packet, int bytes) {
        lock_guard<recursive_mutex> lck(_mtx);
        if(_onts){
            _onts(packet,bytes);
        }

        if(_file){
            fwrite(packet,bytes,1,_file.get());
        }
    }
private:
    void  *_context = nullptr;
    char *_tsbuf[188];
    unordered_map<int,int > _track_id_to_stream_id;
    unordered_map<int,int > _allTrackMap;
    recursive_mutex _mtx;
    onTsCallback _onts;

    char _file_buf[64 * 1024];
    std::shared_ptr<FILE> _file;
};

}//namespace mediakit
#endif //TSMUXER_H
