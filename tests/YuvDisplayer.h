/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef YUVDISPLAYER_H_
#define YUVDISPLAYER_H_
#include <stdexcept>
#include "Util/onceToken.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "SDL2/SDL.h"
#include "libavcodec/avcodec.h"
#ifdef __cplusplus
}
#endif

#if defined(_WIN32)
#pragma comment(lib,"SDL2.lib")
#endif //defined(_WIN32)

using namespace toolkit;
using namespace mediakit;

#define REFRESH_EVENT   (SDL_USEREVENT + 1)


class SDLDisplayerHelper
{
public:
    static SDLDisplayerHelper &Instance(){
        static SDLDisplayerHelper *instance(new SDLDisplayerHelper);
        return *instance;
    }
    static void Destory(){
        delete &Instance();
    }
    template<typename FUN>
    void doTask(FUN &&f){
        {
            lock_guard<mutex> lck(_mtxTask);
            _taskList.emplace_back(f);
        }
        SDL_Event event;
        event.type = REFRESH_EVENT;
        SDL_PushEvent(&event);
    }

    void runLoop(){
        bool flag = true;
        std::function<bool ()> task;
        SDL_Event event;
        while(flag){
            SDL_WaitEvent(&event);
            switch (event.type){
                case REFRESH_EVENT:{
                    {
                        lock_guard<mutex> lck(_mtxTask);
                        if(_taskList.empty()){
                            //not reachable
                            continue;
                        }
                        task = _taskList.front();
                        _taskList.pop_front();
                    }
                    flag = task();
                    break;
                case SDL_QUIT:
                    InfoL << event.type;
                    return;
                }
                default:
                    break;
            }
        }
    }

    void shutdown(){
        doTask([](){return false;});
    }
private:
    SDLDisplayerHelper(){
    };
    ~SDLDisplayerHelper(){
        shutdown();
    };
private:
    std::deque<std::function<bool ()> > _taskList;
    std::mutex _mtxTask;

};


class YuvDisplayer {
public:
    YuvDisplayer(void *hwnd = nullptr,const char *title = "untitled"){

        static onceToken token([]() {
            if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) == -1) {
                string err = "初始化SDL失败:";
                err+= SDL_GetError();
                ErrorL << err;
                throw std::runtime_error(err);
            }
            SDL_LogSetAllPriority(SDL_LOG_PRIORITY_CRITICAL);
            SDL_LogSetOutputFunction([](void *userdata, int category, SDL_LogPriority priority, const char *message){
                DebugL << category << " " <<  priority << message;
            },nullptr);
            InfoL << "SDL_Init";
        }, []() {
            SDLDisplayerHelper::Destory();
            SDL_Quit();
        });

        _title = title;
        _hwnd = hwnd;
    }
    virtual ~YuvDisplayer(){
        if (_texture) {
            SDL_DestroyTexture(_texture);
            _texture = nullptr;
        }
        if (_render) {
            SDL_DestroyRenderer(_render);
            _render = nullptr;
        }
        if (_win) {
            SDL_DestroyWindow(_win);
            _win = nullptr;
        }
    }
    bool displayYUV(AVFrame *pFrame){
        if (!_win) {
            if (_hwnd) {
                _win = SDL_CreateWindowFrom(_hwnd);
            }else {
                _win = SDL_CreateWindow(_title.data(),
                                        SDL_WINDOWPOS_UNDEFINED,
                                        SDL_WINDOWPOS_UNDEFINED,
                                        pFrame->width,
                                        pFrame->height,
                                        SDL_WINDOW_OPENGL);
            }
        }
        if (_win && ! _render){
#if 0
            SDL_RENDERER_SOFTWARE = 0x00000001,         /**< The renderer is a software fallback */
                    SDL_RENDERER_ACCELERATED = 0x00000002,      /**< The renderer uses hardware
                                                                     acceleration */
                    SDL_RENDERER_PRESENTVSYNC = 0x00000004,     /**< Present is synchronized
                                                                     with the refresh rate */
                    SDL_RENDERER_TARGETTEXTURE = 0x00000008     /**< The renderer supports
                                                                     rendering to texture */
#endif

            _render = SDL_CreateRenderer(_win, -1, SDL_RENDERER_ACCELERATED);
        }
        if (_render && !_texture) {
            _texture = SDL_CreateTexture(_render, SDL_PIXELFORMAT_IYUV,
                                         SDL_TEXTUREACCESS_STREAMING,
                                         pFrame->width,
                                         pFrame->height);
        }
        if (_texture) {
            SDL_UpdateYUVTexture(_texture, nullptr,
                                 pFrame->data[0], pFrame->linesize[0],
                                 pFrame->data[1], pFrame->linesize[1],
                                 pFrame->data[2], pFrame->linesize[2]);

            //SDL_UpdateTexture(_texture, nullptr, pFrame->data[0], pFrame->linesize[0]);
            SDL_RenderClear(_render);
            SDL_RenderCopy(_render, _texture, nullptr, nullptr);
            SDL_RenderPresent(_render);
            return true;
        }
        return false;
    }
private:
    string _title;
    SDL_Window *_win = nullptr;
    SDL_Renderer *_render = nullptr;
    SDL_Texture *_texture = nullptr;
    void *_hwnd = nullptr;
};

#endif /* YUVDISPLAYER_H_ */