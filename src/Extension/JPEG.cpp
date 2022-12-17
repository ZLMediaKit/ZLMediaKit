#include "JPEG.h"
#include "Rtsp/Rtsp.h"
#include "Util/util.h"

using namespace toolkit;

namespace mediakit {

class JPEGSdp : public Sdp {
public:
    JPEGSdp(int bitrate): Sdp(90000, Rtsp::PT_JPEG) {
        _printer << "m=video 0 RTP/AVP " << (int)getPayloadType() << "\r\n";
        if (bitrate) {
            _printer << "b=AS:" << bitrate << "\r\n";
        }
        _printer << "a=control:trackID=" << (int)TrackVideo << "\r\n";
    }

    std::string getSdp() const { return _printer; }

    CodecId getCodecId() const { return CodecJPEG; }

private:
    _StrPrinter _printer;
};

Sdp::Ptr JPEGTrack::getSdp() {
    return std::make_shared<JPEGSdp>(getBitRate() / 1024);
}
} // namespace mediakit
