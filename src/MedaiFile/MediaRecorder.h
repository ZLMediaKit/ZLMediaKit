/*
 * MediaRecorder.h
 *
 *  Created on: 2016年12月8日
 *      Author: xzl
 */

#ifndef SRC_MEDAIFILE_MEDIARECORDER_H_
#define SRC_MEDAIFILE_MEDIARECORDER_H_

#include <memory>
#include "Player/PlayerBase.h"
#ifdef  ENABLE_MEDIAFILE
#include "Mp4Maker.h"
#include "HLSMaker.h"
#endif //ENABLE_MEDIAFILE

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
#ifdef  ENABLE_MEDIAFILE
	std::shared_ptr<HLSMaker> m_hlsMaker;
	std::shared_ptr<Mp4Maker> m_mp4Maker;
#endif //ENABLE_MEDIAFILE

};

} /* namespace MediaFile */
} /* namespace ZL */

#endif /* SRC_MEDAIFILE_MEDIARECORDER_H_ */
