/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MPEGVIDEOSEQUENCE_H
#define ZLMEDIAKIT_MPEGVIDEOSEQUENCE_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace mediakit {

/**
 * MPEG-1/2 Video 公共 sequence header 流式解析器。
 * Streaming parser for the common MPEG-1/2 Video sequence header.
 *
 * 解析器只保存起始码状态和 4 字节 header，允许合法 header 跨 Frame/PES。
 * It retains only start-code state and four header bytes, allowing a valid header
 * to span Frame/PES boundaries.
 */
class MpegVideoSequenceParser {
public:
    bool input(const uint8_t *data, size_t size) {
        if (_parsed || !data) {
            return _parsed;
        }
        for (size_t i = 0; i < size && !_parsed; ++i) {
            inputByte(data[i]);
        }
        return _parsed;
    }

    bool parsed() const { return _parsed; }
    int width() const { return _width; }
    int height() const { return _height; }
    float fps() const { return _fps; }

private:
    enum class State : uint8_t {
        Searching,
        StartCodeValue,
        SequenceHeader
    };

    static float getFrameRate(uint8_t code) {
        switch (code) {
            case 1: return 24000.0f / 1001.0f;
            case 2: return 24.0f;
            case 3: return 25.0f;
            case 4: return 30000.0f / 1001.0f;
            case 5: return 30.0f;
            case 6: return 50.0f;
            case 7: return 60000.0f / 1001.0f;
            case 8: return 60.0f;
            default: return 0.0f;
        }
    }

    void inputByte(uint8_t byte) {
        switch (_state) {
            case State::Searching: {
                if (byte == 0x00) {
                    if (_zero_count < 3) {
                        ++_zero_count;
                    }
                    return;
                }
                if (byte == 0x01 && _zero_count >= 2) {
                    _state = State::StartCodeValue;
                    _zero_count = 0;
                    return;
                }
                _zero_count = 0;
                return;
            }
            case State::StartCodeValue: {
                _state = State::Searching;
                _zero_count = 0;
                if (byte == 0xB3) {
                    _state = State::SequenceHeader;
                    _header_size = 0;
                } else if (byte == 0x00) {
                    _zero_count = 1;
                }
                return;
            }
            case State::SequenceHeader: {
                _header[_header_size++] = byte;
                if (_header_size == _header.size()) {
                    parseHeader();
                }
                return;
            }
        }
    }

    void parseHeader() {
        auto width = (_header[0] << 4) | ((_header[1] >> 4) & 0x0F);
        auto height = ((_header[1] & 0x0F) << 8) | _header[2];
        auto fps = getFrameRate(_header[3] & 0x0F);
        if (width && height && fps > 0) {
            _width = width;
            _height = height;
            _fps = fps;
            _parsed = true;
            return;
        }

        // 损坏的 header 可能同时包含下一个起始码，重新扫描这 4 字节避免丢失重叠匹配。
        // A damaged header may contain the next start code; rescan these four bytes
        // so an overlapping match is not lost.
        auto header = _header;
        _state = State::Searching;
        _zero_count = 0;
        _header_size = 0;
        for (auto byte : header) {
            inputByte(byte);
        }
    }

private:
    bool _parsed = false;
    State _state = State::Searching;
    uint8_t _zero_count = 0;
    std::array<uint8_t, 4> _header = {{ 0 }};
    size_t _header_size = 0;
    int _width = 0;
    int _height = 0;
    float _fps = 0;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_MPEGVIDEOSEQUENCE_H
