#ifndef ZLMEDIAKIT_JPEGRTP_H
#define ZLMEDIAKIT_JPEGRTP_H

#include "Rtsp/RtpCodec.h"
#include "Extension/Frame.h"

namespace mediakit {

/**
 * RTP/JPEG specific private data.
 */
struct PayloadContext {
    std::string frame;         ///< current frame buffer
    uint32_t timestamp;      ///< current frame timestamp
    int hdr_size;       ///< size of the current frame header
    uint8_t qtables[128][128];
    uint8_t qtables_len[128];
};

/**
 * 通用 rtp解码类
 */
class JPEGRtpDecoder : public RtpCodec {
public:
    typedef std::shared_ptr <JPEGRtpDecoder> Ptr;

    JPEGRtpDecoder();

    /**
     * 输入rtp并解码
     * @param rtp rtp数据包
     * @param key_pos 此参数内部强制转换为false,请忽略之
     */
    bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos = false) override;

private:
    struct PayloadContext _ctx;
};

class JPEGRtpEncoder : public RtpCodec {
public:
    using Ptr = std::shared_ptr<JPEGRtpEncoder>;

    bool inputFrame(const Frame::Ptr &frame) override;

private:
    void rtpSendJpeg(const uint8_t *buf, int size, uint64_t pts, uint8_t type);
};
}//namespace mediakit
#endif //ZLMEDIAKIT_JPEGRTP_H
