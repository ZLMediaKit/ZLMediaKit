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
#include <utility>

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

static bool accepts_leading_numbers(size_t count) {
    string data;
    for (size_t i = 0; i < count; ++i) {
        data.append("\x00\x00\x00\x00\x00\x00\x00\x00\x00", 9);
    }
    data.append("\x02\x00\x0AonMetaData", 13);
    BufferLikeString input(std::move(data));
    AMFDecoder decoder(input, 0);
    return amfLoadLeadingString(decoder) == "onMetaData";
}

static bool rejects_leading_value(const string &value) {
    BufferLikeString input(value + string("\x02\x00\x0AonMetaData", 13));
    AMFDecoder decoder(input, 0);
    try {
        amfLoadLeadingString(decoder);
    } catch (const runtime_error &ex) {
        return string(ex.what()) == "Expected a string";
    }
    return false;
}

static bool accepts_amf3_string(bool switched_from_amf0) {
    string data;
    if (switched_from_amf0) {
        data.push_back('\x11');
    }
    data.append("\x06\x15onMetaData", 12);
    BufferLikeString input(std::move(data));
    AMFDecoder decoder(input, 0, switched_from_amf0 ? 0 : 3);
    return amfLoadLeadingString(decoder) == "onMetaData";
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

    for (size_t count = 0; count <= 2; ++count) {
        if (!accepts_leading_numbers(count)) {
            cerr << "AMF metadata with " << count << " leading numbers was not decoded" << endl;
            return 3;
        }
    }

    // Only the observed, bounded compatibility prefix is accepted. In
    // particular, complex values must not be materialized just to find a later
    // string, and an arbitrary number of scalar values must not be skipped.
    if (!rejects_leading_value(string("\x0A\x00\x00\x00\x01\x05", 6))) {
        cerr << "complex leading AMF value was not rejected" << endl;
        return 4;
    }
    if (!rejects_leading_value(string(27, '\0'))) {
        cerr << "more than two leading AMF numbers were not rejected" << endl;
        return 5;
    }
    if (!accepts_amf3_string(false) || !accepts_amf3_string(true)) {
        cerr << "standard AMF3 string was not decoded" << endl;
        return 6;
    }

    return 0;
}
