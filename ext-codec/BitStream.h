/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_BITSTREAM_H
#define ZLMEDIAKIT_BITSTREAM_H

#include "Common/macros.h"

namespace mediakit {

class BitStreamReader {
public:
    void setup(const void *data, size_t size) {
        assert(size > 0);
        _bit_index = 0;
        _bit_size = size << 3;
        _buffer = reinterpret_cast<const uint8_t *>(data);
    }

    size_t get_bit_index() const { return _bit_index; }

    uint32_t get_bits(size_t n) {
        uint32_t ret = 0;
        assert(n > 0 && n <= 32);
        while (n--) {
            ret <<= 1;
            ret |= get_bits1();
        }
        return ret;
    }

protected:
    uint8_t get_bits1() {
        if (_bit_index >= _bit_size) {
            throw std::invalid_argument("BitStreamReader out of range");
        }
        auto index = _bit_index;
        uint8_t result = _buffer[index >> 3];
#if __BYTE_ORDER == __BIG_ENDIAN
        result >>= index & 7;
        result &= 1;
#else
        result <<= index & 7;
        result >>= 8 - 1;
#endif
        index++;
        _bit_index = index;
        return result;
    }

protected:
    size_t _bit_size;
    size_t _bit_index = 0;
    const uint8_t *_buffer;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_BITSTREAM_H
