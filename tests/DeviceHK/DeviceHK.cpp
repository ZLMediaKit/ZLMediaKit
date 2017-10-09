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
#ifdef ENABLE_HKDEVICE
#include "DeviceHK.h"
#include "Util/TimeTicker.h"
#include "Util/MD5.h"
namespace ZL {
namespace DEV {

#define HK_APP_NAME  "live"

DeviceHK::DeviceHK() {
	InfoL << endl;
	static onceToken token( []() {
		NET_DVR_Init();
		NET_DVR_SetDVRMessageCallBack_V31([](LONG lCommand,NET_DVR_ALARMER *pAlarmer,char *pAlarmInfo,DWORD dwBufLen,void* pUser){
			WarnL<<pAlarmInfo;
			return TRUE;
		},NULL);
	}, []() {
		NET_DVR_Cleanup();
	});
}

DeviceHK::~DeviceHK() {
	InfoL << endl;
}

void DeviceHK::connectDevice(const connectInfo &info, const connectCB& cb, int iTimeOut) {
	NET_DVR_USER_LOGIN_INFO loginInfo;
	NET_DVR_DEVICEINFO_V40 loginResult;

	//login info
	strcpy(loginInfo.sDeviceAddress, info.strDevIp.c_str());
	loginInfo.wPort = info.ui16DevPort;
	strcpy(loginInfo.sUserName, info.strUserName.c_str());
	strcpy(loginInfo.sPassword, info.strPwd.c_str());

	//callback info
	typedef function< void(LONG lUserID, DWORD dwResult, LPNET_DVR_DEVICEINFO_V30 lpDeviceInfo)> hkLoginCB;
	loginInfo.bUseAsynLogin = TRUE;
	weak_ptr<Device> weakSelf = shared_from_this();
	loginInfo.pUser = new hkLoginCB([weakSelf,cb](LONG lUserID, DWORD dwResult, LPNET_DVR_DEVICEINFO_V30 lpDeviceInfo ) {
						//TraceL<<lUserID<<" "<<dwResult<<" "<<lpDeviceInfo->sSerialNumber;
						connectResult result;
						if(dwResult==TRUE) {
							result.strDevName=(char *)(lpDeviceInfo->sSerialNumber);
							result.ui16ChnStart=lpDeviceInfo->byStartChan;
							result.ui16ChnCount=lpDeviceInfo->byChanNum;
							auto _strongSelf=weakSelf.lock();
							if(_strongSelf) {
								auto strongSelf=dynamic_pointer_cast<DeviceHK>(_strongSelf);
								strongSelf->onConnected(lUserID,lpDeviceInfo);
							}
						} else {
							WarnL<<"connect deviceHK failed:"<<NET_DVR_GetLastError();
						}
						cb(dwResult==TRUE,result);
					});
	loginInfo.cbLoginResult = [](LONG lUserID, DWORD dwResult, LPNET_DVR_DEVICEINFO_V30 lpDeviceInfo , void* pUser) {
				auto *fun=static_cast<hkLoginCB *>(pUser);
				(*fun)(lUserID,dwResult,lpDeviceInfo);
				delete fun;
			};
	NET_DVR_SetConnectTime(iTimeOut * 1000, 3);
	NET_DVR_Login_V40(&loginInfo, &loginResult);
}

void DeviceHK::disconnect(const relustCB& cb) {
	m_mapChannels.clear();
	if (m_i64LoginId >= 0) {
		NET_DVR_Logout(m_i64LoginId);
		m_i64LoginId = -1;
		Device::onDisconnected(true);
	}

}

void DeviceHK::addChannel(int iChn, bool bMainStream) {
	DevChannel::Ptr channel( new DevChannelHK(m_i64LoginId, (char *) m_deviceInfo.sSerialNumber, iChn, bMainStream));
	m_mapChannels[iChn] = channel;
}

void DeviceHK::delChannel(int chn) {
	m_mapChannels.erase(chn);
}

void DeviceHK::onConnected(LONG lUserID, LPNET_DVR_DEVICEINFO_V30 lpDeviceInfo) {
	m_i64LoginId = lUserID;
	m_deviceInfo = *lpDeviceInfo;
	Device::onConnected();
}

void DeviceHK::addAllChannel(bool bMainStream) {
	InfoL << endl;
	for (int i = 0; i < m_deviceInfo.byChanNum; i++) {
		addChannel(m_deviceInfo.byStartChan + i, bMainStream);
	}
}

DevChannelHK::DevChannelHK(int64_t i64LoginId, const char* pcDevName, int iChn, bool bMainStream) :
		DevChannel(HK_APP_NAME,(StrPrinter<<MD5(pcDevName).hexdigest()<<"_"<<iChn<<endl).data()),
		m_i64LoginId(i64LoginId) {
	InfoL << endl;
	NET_DVR_PREVIEWINFO previewInfo;
	previewInfo.lChannel = iChn; //通道号
	previewInfo.dwStreamType = bMainStream ? 0 : 1; // 码流类型，0-主码流，1-子码流，2-码流3，3-码流4 等以此类推
	previewInfo.dwLinkMode = 1; // 0：TCP方式,1：UDP方式,2：多播方式,3 - RTP方式，4-RTP/RTSP,5-RSTP/HTTP
	previewInfo.hPlayWnd = 0; //播放窗口的句柄,为NULL表示不播放图象
	previewInfo.byProtoType = 0; //应用层取流协议，0-私有协议，1-RTSP协议
	previewInfo.dwDisplayBufNum = 1; //播放库播放缓冲区最大缓冲帧数，范围1-50，置0时默认为1
	previewInfo.bBlocked = 0;
	m_i64PreviewHandle = NET_DVR_RealPlay_V40(m_i64LoginId, &previewInfo,
										[](LONG lPlayHandle,DWORD dwDataType,BYTE *pBuffer,DWORD dwBufSize,void* pUser) {
											DevChannelHK *self=reinterpret_cast<DevChannelHK *>(pUser);
											if(self->m_i64PreviewHandle!=(int64_t)lPlayHandle) {
												return;
											}
											self->onPreview(dwDataType,pBuffer,dwBufSize);
										}, this);
	if (m_i64PreviewHandle == -1) {
		throw std::runtime_error( StrPrinter 	<< "设备[" << pcDevName << "/" << iChn << "]开始实时预览失败:"
												<< NET_DVR_GetLastError() << endl);
	}
}

DevChannelHK::~DevChannelHK() {
	InfoL << endl;
	if (m_i64PreviewHandle >= 0) {
		NET_DVR_StopRealPlay(m_i64PreviewHandle);
		m_i64PreviewHandle = -1;
	}
	if (m_iPlayHandle >= 0) {
		PlayM4_StopSoundShare(m_iPlayHandle);
		PlayM4_Stop(m_iPlayHandle);
		m_iPlayHandle = -1;
	}
}

void DevChannelHK::onPreview(DWORD dwDataType, BYTE* pBuffer, DWORD dwBufSize) {
	//TimeTicker1(-1);
	switch (dwDataType) {
	case NET_DVR_SYSHEAD: { //系统头数据
		if (!PlayM4_GetPort(&m_iPlayHandle)) {  //获取播放库未使用的通道号
			WarnL << "PlayM4_GetPort:" << NET_DVR_GetLastError();
			break;
		}
		if (dwBufSize > 0) {
			if (!PlayM4_SetStreamOpenMode(m_iPlayHandle, STREAME_REALTIME)) { //设置实时流播放模式
				WarnL << "PlayM4_SetStreamOpenMode:" << NET_DVR_GetLastError();
				break;
			}
			if (!PlayM4_OpenStream(m_iPlayHandle, pBuffer, dwBufSize,
					1024 * 1024)) {  //打开流接口
				WarnL << "PlayM4_OpenStream:" << NET_DVR_GetLastError();
				break;
			}

			PlayM4_SetDecCallBackMend(m_iPlayHandle,
					[](int nPort,char * pBuf,int nSize,FRAME_INFO * pFrameInfo, void* nUser,int nReserved2) {
						DevChannelHK *chn=reinterpret_cast<DevChannelHK *>(nUser);
						if(chn->m_iPlayHandle!=nPort) {
							return;
						}
						chn->onGetDecData(pBuf,nSize,pFrameInfo);
					}, this);
			if (!PlayM4_Play(m_iPlayHandle, 0)) {  //播放开始
				WarnL << "PlayM4_Play:" << NET_DVR_GetLastError();
				break;
			}
			InfoL << "设置解码器成功！" << endl;
			//打开音频解码, 需要码流是复合流
			if (!PlayM4_PlaySoundShare(m_iPlayHandle)) {
				WarnL << "PlayM4_PlaySound:" << NET_DVR_GetLastError();
				break;
			}
		}
	}
		break;
	case NET_DVR_STREAMDATA: { //流数据（包括复合流或音视频分开的视频流数据）
		if (dwBufSize > 0 && m_iPlayHandle != -1) {
			if (!PlayM4_InputData(m_iPlayHandle, pBuffer, dwBufSize)) {
				WarnL << "PlayM4_InputData:" << NET_DVR_GetLastError();
				break;
			}
		}
	}
		break;
	case NET_DVR_AUDIOSTREAMDATA: { //音频数据
	}
		break;
	case NET_DVR_PRIVATE_DATA: { //私有数据,包括智能信息
	}
		break;
	default:
		break;
	}
}

void DevChannelHK::onGetDecData(char* pBuf, int nSize, FRAME_INFO* pFrameInfo) {
	//InfoL << pFrameInfo->nType;
	switch (pFrameInfo->nType) {
	case T_YV12: {
		if (!m_bVideoSeted) {
			m_bVideoSeted = true;
			VideoInfo video;
			video.iWidth = pFrameInfo->nWidth;
			video.iHeight = pFrameInfo->nHeight;
			video.iFrameRate = pFrameInfo->nFrameRate;
			initVideo(video);
		}
		char *yuv[3];
		int yuv_len[3];
		yuv_len[0] = pFrameInfo->nWidth;
		yuv_len[1] = pFrameInfo->nWidth / 2;
		yuv_len[2] = pFrameInfo->nWidth / 2;
		int dwOffset_Y = pFrameInfo->nWidth * pFrameInfo->nHeight;
		yuv[0] = pBuf;
		yuv[2] = yuv[0] + dwOffset_Y;
		yuv[1] = yuv[2] + dwOffset_Y / 4;
		inputYUV(yuv, yuv_len, pFrameInfo->nStamp);
	}
		break;
	case T_AUDIO16: {
		if (!m_bAudioSeted) {
			m_bAudioSeted = true;
			AudioInfo audio;
			audio.iChannel = pFrameInfo->nWidth;
			audio.iSampleBit = pFrameInfo->nHeight;
			audio.iSampleRate = pFrameInfo->nFrameRate;
			initAudio(audio);
		}
		inputPCM(pBuf, nSize, pFrameInfo->nStamp);
	}
		break;
	default:
		break;
	}
}

} /* namespace DEV */
} /* namespace ZL */

#endif //ENABLE_HKDEVICE
