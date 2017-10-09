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

#ifndef DEVICE_DEVICEHK_H_
#define DEVICE_DEVICEHK_H_

#include <sys/time.h>
#include "HCNetSDK.h"
#include "PlayM4.h"
#include "Device/Device.h"
#include "Util/onceToken.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"

using namespace ZL::Util;

namespace ZL {
namespace DEV {

class connectInfo {
public:
	connectInfo(const char *_strDevIp,
				uint16_t _ui16DevPort,
				const char *_strUserName,
				const char *_strPwd) {
		strDevIp = _strDevIp;
		ui16DevPort = _ui16DevPort;
		strUserName = _strUserName;
		strPwd = _strPwd;
	}
	string strDevIp;
	uint16_t ui16DevPort;
	string strUserName;
	string strPwd;
};

class connectResult {
public:
	string strDevName;
	uint16_t ui16ChnStart;
	uint16_t ui16ChnCount;
};

typedef function<void(bool success, const connectResult &)> connectCB;
typedef function<void(bool success)> relustCB;

class Device: public enable_shared_from_this<Device> {
public:
	typedef std::shared_ptr<Device> Ptr;
	Device() {
	}
	virtual ~Device(){ disconnect([](bool bSuccess){
	});};

	virtual void connectDevice(const connectInfo &info, const connectCB &cb, int iTimeOut = 3)=0;

	virtual void disconnect(const relustCB &cb) {
	}

	virtual void addChannel(int iChnIndex, bool bMainStream = true)=0;

	virtual void delChannel(int iChnIndex)=0;

	virtual void addAllChannel(bool bMainStream = true)=0;

protected:
	void onConnected() {
	}
	void onDisconnected(bool bSelfDisconnect) {
	}

};


class DevChannelHK;
class DeviceHK: public Device {
public:
	typedef std::shared_ptr<DeviceHK> Ptr;
	DeviceHK();
	virtual ~DeviceHK();

	void connectDevice(const connectInfo &info, const connectCB &cb, int iTimeOut = 3) override;
	void disconnect(const relustCB &cb) override;

	void addChannel(int iChnIndex, bool bMainStream = true) override;
	void delChannel(int iChnIndex) override;
	void addAllChannel(bool bMainStream = true) override;
private:
	map<int, std::shared_ptr<DevChannel> > m_mapChannels;
	int64_t m_i64LoginId = -1;
	NET_DVR_DEVICEINFO_V30 m_deviceInfo;
	void onConnected(LONG lUserID, LPNET_DVR_DEVICEINFO_V30 lpDeviceInfo);
};

class DevChannelHK: public DevChannel {
public:
	typedef std::shared_ptr<DevChannel> Ptr;
	DevChannelHK(int64_t i64LoginId, const char *pcDevName, int iChn, bool bMainStream = true);
	virtual ~DevChannelHK();
protected:
	int64_t m_i64LoginId = -1;
	int64_t m_i64PreviewHandle = -1;
	int m_iPlayHandle = -1;
	void onPreview(DWORD dwDataType, BYTE *pBuffer, DWORD dwBufSize);
	void onGetDecData(char * pBuf, int nSize, FRAME_INFO * pFrameInfo);
	bool m_bVideoSeted = false;
	bool m_bAudioSeted = false;
};

} /* namespace DEV */
} /* namespace ZL */

#endif /* DEVICE_DEVICEHK_H_ */
