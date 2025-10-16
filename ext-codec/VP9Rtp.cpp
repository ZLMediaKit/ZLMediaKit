/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "VP9Rtp.h"
#include "Extension/Frame.h"
#include "Common/config.h"

namespace mediakit{

const int16_t kNoPictureId = -1;
const int8_t  kNoTl0PicIdx = -1;
const uint8_t kNoTemporalIdx = 0xFF;
const int kNoKeyIdx = -1;

struct VP9ResolutionLayer {
    int width;
    int height;
};

struct RTPPayloadVP9 {
    bool hasPictureID = false;
    bool interPicturePrediction = false;
    bool hasLayerIndices = false;
    bool flexibleMode = false;
    bool beginningOfLayerFrame = false;
    bool endingOfLayerFrame = false;
    bool hasScalabilityStructure = false;
    bool largePictureID = false;
    int pictureID = -1;
    int temporalID = -1;
    bool isSwitchingUp = false;
    int spatialID = -1;
    bool isInterLayeredDepUsed = false;
    int tl0PicIdx = -1;
    int referenceIdx = -1;
    bool additionalReferenceIdx = false;
    int spatialLayers = -1;
    bool hasResolution = false;
    bool hasGof = false;
    int numberOfFramesInGof = -1;
    std::vector<VP9ResolutionLayer> resolutions;
    int parse(unsigned char* data, int dataLength);
    bool keyFrame() const { return beginningOfLayerFrame && !interPicturePrediction; }
    std::string dump() const { 
        char line[64] = {0};
        snprintf(line, sizeof(line), "%c%c%c%c%c%c%c- %d %d, %d %d", 
            hasPictureID ? 'I' : ' ', 
            interPicturePrediction ? 'P' : ' ', 
            hasLayerIndices ? 'L' : ' ', 
            flexibleMode ? 'F' : ' ',
            beginningOfLayerFrame ? 'B' : ' ', 
            endingOfLayerFrame ? 'E' : ' ', 
            hasScalabilityStructure ? 'V' : ' ',
            pictureID, tl0PicIdx,
            spatialID, temporalID);
        return line;
    }
};
//
// VP9 format:
//
// Payload descriptor (Flexible mode F = 1)
//        0 1 2 3 4 5 6 7
//       +-+-+-+-+-+-+-+-+
//       |I|P|L|F|B|E|V|-| (REQUIRED)
//       +-+-+-+-+-+-+-+-+
//  I:   |M| PICTURE ID  | (REQUIRED)
//       +-+-+-+-+-+-+-+-+
//  M:   | EXTENDED PID  | (RECOMMENDED)
//       +-+-+-+-+-+-+-+-+
//  L:   |  T  |U|  S  |D| (CONDITIONALLY RECOMMENDED)
//       +-+-+-+-+-+-+-+-+                             -
//  P,F: | P_DIFF      |N| (CONDITIONALLY REQUIRED)    - up to 3 times
//       +-+-+-+-+-+-+-+-+                             -
//  V:   | SS            |
//       | ..            |
//       +-+-+-+-+-+-+-+-+
//
// Payload descriptor (Non flexible mode F = 0)
//
//       0 1 2 3 4 5 6 7
//      +-+-+-+-+-+-+-+-+
//      |I|P|L|F|B|E|V|-| (REQUIRED)
//      +-+-+-+-+-+-+-+-+
// I:   |M| PICTURE ID  | (RECOMMENDED)
//      +-+-+-+-+-+-+-+-+
// M:   | EXTENDED PID  | (RECOMMENDED)
//      +-+-+-+-+-+-+-+-+
// L:   |  T  |U|  S  |D| (CONDITIONALLY RECOMMENDED)
//      +-+-+-+-+-+-+-+-+
//      |   TL0PICIDX   | (CONDITIONALLY REQUIRED)
//      +-+-+-+-+-+-+-+-+
// V:   | SS            |
//      | ..            |
//      +-+-+-+-+-+-+-+-+
#define kIBit 0x80
#define kPBit 0x40
#define kLBit 0x20
#define kFBit 0x10
#define kBBit 0x08
#define kEBit 0x04
#define kVBit 0x02
int RTPPayloadVP9::parse(unsigned char *data, int dataLength) {
  const unsigned char* dataPtr = data;
  // Parse mandatory first byte of payload descriptor
  this->hasPictureID = (*dataPtr & kIBit); // I bit
  this->interPicturePrediction = (*dataPtr & kPBit); // P bit
  this->hasLayerIndices = (*dataPtr & kLBit); // L bit
  this->flexibleMode = (*dataPtr & kFBit); // F bit
  this->beginningOfLayerFrame = (*dataPtr & kBBit); // B bit
  this->endingOfLayerFrame = (*dataPtr & kEBit); // E bit
  this->hasScalabilityStructure = (*dataPtr & kVBit); // V bit
  dataPtr++;

  if (this->hasPictureID) {
    this->largePictureID = (*dataPtr & 0x80);  // M bit
    this->pictureID = (*dataPtr & 0x7F);
    if (this->largePictureID) {
      dataPtr++;
      this->pictureID = ntohs((this->pictureID << 16) + (*dataPtr & 0xFF));
    }
    dataPtr++;
  }

  if (this->hasLayerIndices) {
    this->temporalID = (*dataPtr & 0xE0) >> 5;  // T bits
    this->isSwitchingUp = (*dataPtr & 0x10);  // U bit
    this->spatialID = (*dataPtr & 0x0E) >> 1;  // S bits
    this->isInterLayeredDepUsed = (*dataPtr & 0x01);  // D bit
    if (this->flexibleMode) { // marked in webrtc code
      do {
        dataPtr++;
        this->referenceIdx = (*dataPtr & 0xFE) >> 1;
        this->additionalReferenceIdx = (*dataPtr & 0x01);  // D bit
      } while (this->additionalReferenceIdx);
    } else {
      dataPtr++;
      this->tl0PicIdx = (*dataPtr & 0xFF);
    }
    dataPtr++;
  }

  if (this->flexibleMode && this->interPicturePrediction) {
      /* Skip reference indices */
      uint8_t nbit;
      do {
          uint8_t p_diff = (*dataPtr & 0xFE) >> 1;
          nbit = (*dataPtr & 0x01);
          dataPtr++;
      } while (nbit);
  }
  if (this->hasScalabilityStructure) {
    this->spatialLayers = (*dataPtr & 0xE0) >> 5;  // N_S bits
    this->hasResolution = (*dataPtr & 0x10);  // Y bit
    this->hasGof = (*dataPtr & 0x08);  // G bit
    dataPtr++;
    if (this->hasResolution) {
      for (int i = 0; i <= this->spatialLayers; i++) {
        int width = (dataPtr[0] << 8) + dataPtr[1];
        dataPtr += 2;
        int height = (dataPtr[0] << 8) + dataPtr[1];
        dataPtr += 2;
        // InfoL << "got vp9 " << width << "x" << height;
        this->resolutions.push_back({ width, height });
      }
    }
    if (this->hasGof) {
      this->numberOfFramesInGof = *dataPtr & 0xFF;  // N_G bits
      dataPtr++;
      for (int frame_index = 0; frame_index < this->numberOfFramesInGof; frame_index++) {
        // TODO(javierc): Read these values if needed
        int reference_indices = (*dataPtr & 0x0C) >> 2;  // R bits
        dataPtr++;
        for (int reference_index = 0; reference_index < reference_indices; reference_index++) {
          dataPtr++;
        }
      }
    }
  }

  return dataPtr - data;
}


////////////////////////////////////////////////////
VP9RtpDecoder::VP9RtpDecoder() {
    obtainFrame();
}

void VP9RtpDecoder::obtainFrame() {
    _frame = FrameImp::create<VP9Frame>();
}

bool VP9RtpDecoder::inputRtp(const RtpPacket::Ptr &rtp, bool key_pos) {
    auto seq = rtp->getSeq();
    bool is_gop = decodeRtp(rtp);
    if (!_gop_dropped && seq != (uint16_t)(_last_seq + 1) && _last_seq) {
        _gop_dropped = true;
        WarnL << "start drop VP9 gop, last seq:" << _last_seq << ", rtp:\r\n" << rtp->dumpString();
    }
    _last_seq = seq;
    return is_gop;
}

bool VP9RtpDecoder::decodeRtp(const RtpPacket::Ptr &rtp) {
    auto payload_size = rtp->getPayloadSize();
    if (payload_size < 1) {
        // No actual payload
        return false;
    }
    auto payload = rtp->getPayload();
    auto stamp = rtp->getStampMS();
    auto seq = rtp->getSeq();

    RTPPayloadVP9 info;
    int offset = info.parse(payload, payload_size);
    // InfoL << rtp->dumpString() << "\n" << info.dump();
    bool start = info.beginningOfLayerFrame;
    if (start) {
        _frame->_pts = stamp;
        _frame->_buffer.clear();
        _frame_drop = false;
    }

    if (_frame_drop) {
        // This frame is incomplete
        return false;
    }

    if (!start && seq != (uint16_t)(_last_seq + 1)) {
        // 中间的或末尾的rtp包，其seq必须连续，否则说明rtp丢包，那么该帧不完整，必须得丢弃
        _frame_drop = true;
        _frame->_buffer.clear();
        return false;
    }
    // Append data
    _frame->_buffer.append((char *)payload + offset, payload_size - offset);
    if (info.endingOfLayerFrame) { // rtp->getHeader()->mark
        // 确保下一个包必须是beginningOfLayerFrame
        _frame_drop = true;
        // 该帧最后一个rtp包,输出frame
        outputFrame(rtp);
    }
    return info.keyFrame();
}

void VP9RtpDecoder::outputFrame(const RtpPacket::Ptr &rtp) {
    if (_frame->dropAble()) {
        // 不参与dts生成  [AUTO-TRANSLATED:dff3b747]
        // Not involved in dts generation
        _frame->_dts = _frame->_pts;
    } else {
        // rtsp没有dts，那么根据pts排序算法生成dts  [AUTO-TRANSLATED:f37c17f3]
        // Rtsp does not have dts, so dts is generated according to the pts sorting algorithm
        _dts_generator.getDts(_frame->_pts, _frame->_dts);
    }

    if (_frame->keyFrame() && _gop_dropped) {
        _gop_dropped = false;
        InfoL << "new gop received, rtp:\r\n" << rtp->dumpString();
    }
    if (!_gop_dropped || _frame->configFrame()) {
        // InfoL << _frame->pts() << " size=" << _frame->size();
        RtpCodec::inputFrame(_frame);
    }
    obtainFrame();
}


////////////////////////////////////////////////////////////////////////

bool VP9RtpEncoder::inputFrame(const Frame::Ptr &frame) {
    uint8_t header[20] = { 0 };
    int nheader = 1;
    header[0] = kBBit;
    bool key = frame->keyFrame();
    if (!key)
        header[0] |= kPBit;
#if 1
    header[0] |= kIBit;
    if (++_pic_id > 0x7FFF) {
        _pic_id = 0;
    }
    header[1] = (0x80 | ((_pic_id >> 8) & 0x7F));
    header[2] = (_pic_id & 0xFF);
    nheader += 2;
#endif
    const char *ptr = frame->data() + frame->prefixSize();
    int len = frame->size() - frame->prefixSize();
    int pdu_size = getRtpInfo().getMaxSize() - nheader;

    bool mark = false;
    for (size_t pos = 0; pos < len; pos += pdu_size) {
        if (len - pos <= pdu_size) {
            pdu_size = len - pos;
            header[0] |= kEBit;
            mark = true;
        }

        auto rtp = getRtpInfo().makeRtp(TrackVideo, nullptr, pdu_size + nheader, mark, frame->pts());
        if (rtp) {
            uint8_t *payload = rtp->getPayload();
            memcpy(payload, header, nheader);
            memcpy(payload + nheader, ptr + pos, pdu_size);
            RtpCodec::inputRtp(rtp, key);
        }
        key = false;
        header[0] &= (~kBBit); //  Clear 'Begin of partition' bit.
    }
    return true;
}

} // namespace mediakit
