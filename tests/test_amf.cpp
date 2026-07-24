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

enum class ContainerType {
    Object,
    EcmaArray,
    StrictArray,
};

static const char *container_name(ContainerType type) {
    switch (type) {
    case ContainerType::Object:
        return "object";
    case ContainerType::EcmaArray:
        return "ECMA array";
    case ContainerType::StrictArray:
        return "strict array";
    }
    return "unknown";
}

static AMFType container_amf_type(ContainerType type) {
    switch (type) {
    case ContainerType::Object:
        return AMF_OBJECT;
    case ContainerType::EcmaArray:
        return AMF_ECMA_ARRAY;
    case ContainerType::StrictArray:
        return AMF_STRICT_ARRAY;
    }
    return AMF_UNDEFINED;
}

static string make_nested_container(ContainerType type, size_t depth) {
    string result;
    for (size_t i = 0; i < depth; ++i) {
        switch (type) {
        case ContainerType::Object:
            result.append("\x03\x00\x01x", 4);
            break;
        case ContainerType::EcmaArray:
            result.append("\x08\x00\x00\x00\x01\x00\x01x", 8);
            break;
        case ContainerType::StrictArray:
            result.append("\x0A\x00\x00\x00\x01", 5);
            break;
        }
    }
    result.append("\x05", 1);
    if (type != ContainerType::StrictArray) {
        for (size_t i = 0; i < depth; ++i) {
            result.append("\x00\x00\x09", 3);
        }
    }
    return result;
}

static bool accepts_nesting_limit(ContainerType type) {
    BufferLikeString input(make_nested_container(type, 64));
    AMFDecoder decoder(input, 0);
    try {
        return decoder.load<AMFValue>().type() == container_amf_type(type);
    } catch (const runtime_error &) {
        return false;
    }
}

static bool rejects_excessive_nesting(ContainerType type) {
    BufferLikeString input(make_nested_container(type, 65));
    AMFDecoder decoder(input, 0);
    try {
        decoder.load<AMFValue>();
    } catch (const runtime_error &ex) {
        return string(ex.what()) == "Maximum AMF nesting depth exceeded";
    }
    return false;
}

int main() {
    const ContainerType types[] = {
        ContainerType::Object,
        ContainerType::EcmaArray,
        ContainerType::StrictArray,
    };

    for (auto type : types) {
        if (!accepts_nesting_limit(type)) {
            cerr << "AMF " << container_name(type) << " at the nesting limit was not decoded" << endl;
            return 1;
        }
        if (!rejects_excessive_nesting(type)) {
            cerr << "AMF " << container_name(type) << " beyond the nesting limit was not rejected" << endl;
            return 2;
        }
    }

    return 0;
}
