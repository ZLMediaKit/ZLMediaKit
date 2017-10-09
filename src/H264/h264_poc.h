// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_H264_POC_H_
#define MEDIA_VIDEO_H264_POC_H_

#include <stdint.h>
#include "macros.h"

using namespace std;

namespace media {
    
    struct H264SPS;
    struct H264SliceHeader;
    
    class MEDIA_EXPORT H264POC {
    public:
        H264POC();
        ~H264POC();
        
        // Compute the picture order count for a slice, storing the result into
        // |*pic_order_cnt|.
        bool ComputePicOrderCnt(
                                const H264SPS* sps,
                                const H264SliceHeader& slice_hdr,
                                int32_t* pic_order_cnt);
        
        // Reset computation state. It's best (although not strictly required) to call
        // this after a seek.
        void Reset();
        
    private:
        int32_t ref_pic_order_cnt_msb_;
        int32_t ref_pic_order_cnt_lsb_;
        int32_t prev_frame_num_;
        int32_t prev_frame_num_offset_;
        
        DISALLOW_COPY_AND_ASSIGN(H264POC);
    };
    
}  // namespace media

#endif  // MEDIA_VIDEO_H264_POC_H_
