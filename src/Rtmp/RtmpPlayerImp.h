/*
 * RtmpPlayerImp.h
 *
 *  Created on: 2016年12月1日
 *      Author: xzl
 */

#ifndef SRC_RTMP_RTMPPLAYERIMP_H_
#define SRC_RTMP_RTMPPLAYERIMP_H_

#include <memory>
#include <functional>
#include "Common/config.h"
#include "RtmpPlayer.h"
#include "RtmpParser.h"
#include "Poller/Timer.h"
#include "Util/TimeTicker.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Player;

namespace ZL {
namespace Rtmp {
class RtmpPlayerImp: public PlayerImp<RtmpPlayer,RtmpParser> {
public:
    typedef std::shared_ptr<RtmpPlayerImp> Ptr;
    RtmpPlayerImp();
    virtual ~RtmpPlayerImp();
    float getProgress() const override{
        if(getDuration() > 0){
            return getProgressTime() / getDuration();
        }
        return PlayerBase::getProgress();
    };
    void seekTo(float fProgress) override{
        fProgress = MAX(float(0),MIN(fProgress,float(1.0)));
        seekToTime(fProgress * getDuration());
    };
private:
    //派生类回调函数
    bool onCheckMeta(AMFValue &val)  override {
        try {
            m_parser.reset(new RtmpParser(val));
            m_parser->setOnVideoCB(m_onGetVideoCB);
            m_parser->setOnAudioCB(m_onGetAudioCB);
            return true;
        } catch (std::exception &ex) {
            WarnL << ex.what();
            return false;
        }
    }
    void onMediaData(RtmpPacket &chunkData) override {
    	if(m_parser){
    		m_parser->inputRtmp(chunkData);
    	}
    }
};
                    
                    
} /* namespace Rtmp */
} /* namespace ZL */
                
#endif /* SRC_RTMP_RTMPPLAYERIMP_H_ */
