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

#ifndef YUVDISPLAYER_H_
#define YUVDISPLAYER_H_
#include <stdexcept>
#include "Util/onceToken.h"
#ifdef __cplusplus
extern "C" {
#endif
#include <SDL/SDL.h>
#include <libavcodec/avcodec.h>
#ifdef __cplusplus
}
#endif

namespace ZL {
namespace Screen {

class YuvDisplayer {
public:
	YuvDisplayer(){
		static onceToken token([]() {
			if(SDL_Init(SDL_INIT_EVERYTHING) == -1) {
				throw std::runtime_error("初始化SDL失败");
			}
		}, []() {
			SDL_Quit();
		});
	}
	virtual ~YuvDisplayer(){
		if(m_pOverlay){
			SDL_FreeYUVOverlay(m_pOverlay);
		}
	}
	bool displayYUV(AVFrame *pFrame){
		if (!m_pScreen) {
			/* Set up the screen */
			m_pScreen = SDL_SetVideoMode(1366, 768, 16, SDL_SWSURFACE);
		}

		if (!m_pOverlay && m_pScreen) {
			/* Create a YUV overlay */
			m_pOverlay = SDL_CreateYUVOverlay(pFrame->width,  pFrame->height, SDL_YV12_OVERLAY, m_pScreen);
			/* Set the window caption */
			SDL_WM_SetCaption("YUV Window", NULL);
		}
		if (m_pOverlay) {
			/* Apply the image to the screen */
			m_pOverlay->pixels[0] = pFrame->data[0];
			m_pOverlay->pixels[2] = pFrame->data[1];
			m_pOverlay->pixels[1] = pFrame->data[2];

			m_pOverlay->pitches[0] = pFrame->linesize[0];
			m_pOverlay->pitches[2] = pFrame->linesize[1];
			m_pOverlay->pitches[1] = pFrame->linesize[2];

			/* Update the screen */
			SDL_Rect rect = { 0 ,0 ,1366,768};
			SDL_LockYUVOverlay(m_pOverlay);
			SDL_DisplayYUVOverlay(m_pOverlay, &rect);
			SDL_UnlockYUVOverlay(m_pOverlay);
			return true;
		}
		return false;
	}
private:
	SDL_Surface* m_pScreen = nullptr;
	SDL_Overlay* m_pOverlay = nullptr;
};

} /* namespace Screen */
} /* namespace ZL */

#endif /* YUVDISPLAYER_H_ */
