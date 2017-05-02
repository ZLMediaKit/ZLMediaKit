/*
 * MediaRecorder.h
 *
 *  Created on: 2016年12月8日
 *      Author: xzl
 */

#ifndef SRC_MEDIAFILE_MEDIARECORDER_H_
#define SRC_MEDIAFILE_MEDIARECORDER_H_

#include <memory>
#include "Player/PlayerBase.h"

#ifdef  ENABLE_MP4V2
#include "Mp4Maker.h"
#endif //ENABLE_MP4V2

#ifdef  ENABLE_HLS
#include "HLSMaker.h"
#endif //ENABLE_HLS

using namespace std;
using namespace ZL::Player;

namespace ZL {
namespace MediaFile {


class MediaRecorder {
public:
	typedef std::shared_ptr<MediaRecorder> Ptr;
	MediaRecorder(const string &strApp,const string &strId,const std::shared_ptr<PlayerBase> &pPlayer);
	virtual ~MediaRecorder();

	void inputH264(	void *pData,
					uint32_t ui32Length,
					uint32_t ui32TimeStamp,
					int iType);

	void inputAAC(	void *pData,
					uint32_t ui32Length,
					uint32_t ui32TimeStamp);
private:

#ifdef  ENABLE_HLS
	std::shared_ptr<HLSMaker> m_hlsMaker;
#endif //ENABLE_HLS

#ifdef  ENABLE_MP4V2
	std::shared_ptr<Mp4Maker> m_mp4Maker;
#endif //ENABLE_MP4V2

};

} /* namespace MediaFile */
} /* namespace ZL */

#endif /* SRC_MEDIAFILE_MEDIARECORDER_H_ */
