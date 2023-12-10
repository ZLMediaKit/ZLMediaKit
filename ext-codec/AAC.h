/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_AAC_H
#define ZLMEDIAKIT_AAC_H

#include "Extension/Frame.h"
#include "Extension/Track.h"
#define ADTS_HEADER_LEN 7

namespace mediakit{

/**
 * aac音频通道
 */
class AACTrack : public AudioTrack {
public:
    using Ptr = std::shared_ptr<AACTrack>;

    AACTrack() = default;

    /**
     * 通过aac extra data 构造对象
     */
    AACTrack(const std::string &aac_cfg);

    bool ready() const override;
    CodecId getCodecId() const override;
    int getAudioChannel() const override;
    int getAudioSampleRate() const override;
    int getAudioSampleBit() const override;
    bool inputFrame(const Frame::Ptr &frame) override;
    toolkit::Buffer::Ptr getExtraData() const override;
    void setExtraData(const uint8_t *data, size_t size) override;
    bool update() override;

private:
    Sdp::Ptr getSdp(uint8_t payload_type) const override;
    Track::Ptr clone() const override;
    bool inputFrame_l(const Frame::Ptr &frame);

private:
    std::string _cfg;
    int _channel = 0;
    int _sampleRate = 0;
    int _sampleBit = 16;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_AAC_H