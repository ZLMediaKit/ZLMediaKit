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

#ifndef H264Decoder_H_
#define H264Decoder_H_
#include <string>
#include <memory>
#include <stdexcept>
#ifdef __cplusplus
extern "C" {
#endif
//#include "libavutil/mathematics.h"
#include "libavcodec/avcodec.h"
//#include "libswscale/swscale.h"
#ifdef __cplusplus
}
#endif

using namespace std;

namespace ZL {
namespace Codec {

class H264Decoder
{
public:
	H264Decoder(void){
		avcodec_register_all();
		AVCodec *pCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
		if (!pCodec) {
			throw std::runtime_error("未找到H264解码器");
		}
		m_pContext.reset(avcodec_alloc_context3(pCodec), [](AVCodecContext *pCtx) {
			avcodec_close(pCtx);
			avcodec_free_context(&pCtx);
		});
		if (!m_pContext) {
			throw std::runtime_error("创建解码器失败");
		}
		if (pCodec->capabilities & CODEC_CAP_TRUNCATED) {
			/* we do not send complete frames */
			m_pContext->flags |= CODEC_FLAG_TRUNCATED;
		}
		if(avcodec_open2(m_pContext.get(), pCodec, NULL)< 0){
			throw std::runtime_error("打开编码器失败");
		}
		m_pFrame.reset(av_frame_alloc(),[](AVFrame *pFrame){
			av_frame_free(&pFrame);
		});
		if (!m_pFrame) {
			throw std::runtime_error("创建帧缓存失败");
		}
	}
	virtual ~H264Decoder(void){}
	bool inputVideo(unsigned char* data,unsigned int dataSize,uint32_t ui32Stamp,AVFrame **ppFrame){
		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = data;
		pkt.size = dataSize;
		pkt.dts = ui32Stamp;
		int iGotPicture ;
		auto iLen = avcodec_decode_video2(m_pContext.get(), m_pFrame.get(), &iGotPicture, &pkt);
		if (!iGotPicture || iLen < 0) {
			return false;
		}
		*ppFrame = m_pFrame.get();
		return true;
	}
private:
	std::shared_ptr<AVCodecContext> m_pContext;
	std::shared_ptr<AVFrame> m_pFrame;
};


} /* namespace Codec */
} /* namespace ZL */

#endif /* H264Decoder_H_ */


