/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include <stdexcept>
#include <string>

#include "Network/Buffer.h"
#include "Rtmp/amf.h"

using namespace std;
using namespace toolkit;

static string make_nested_object(size_t depth) {
    string result;
    for (size_t i = 0; i < depth; ++i) {
        result.append("\x03\x00\x01x", 4);
    }
    result.append("\x05", 1);
    for (size_t i = 0; i < depth; ++i) {
        result.append("\x00\x00\x09", 3);
    }
    return result;
}

int main() {
    {
        BufferLikeString input(make_nested_object(64));
        AMFDecoder decoder(input, 0);
        if (decoder.load<AMFValue>().type() != AMF_OBJECT) {
            cerr << "AMF object at the nesting limit was not decoded" << endl;
            return 1;
        }
    }

    {
        BufferLikeString input(make_nested_object(65));
        AMFDecoder decoder(input, 0);
        bool depth_rejected = false;
        try {
            decoder.load<AMFValue>();
        } catch (const runtime_error &ex) {
            depth_rejected = string(ex.what()) == "Maximum AMF nesting depth exceeded";
        }
        if (!depth_rejected) {
            cerr << "AMF object beyond the nesting limit was not rejected" << endl;
            return 2;
        }
    }

    return 0;
}
