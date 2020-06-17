/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>
#include <stdexcept>
#include "amf.h"
#include "utils.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Network/sockutil.h"
#include "Util/util.h"
using namespace toolkit;

/////////////////////AMFValue/////////////////////////////
inline void AMFValue::destroy() {
    switch (_type) {
    case AMF_STRING:
        if (_value.string) {
            delete _value.string;
            _value.string = nullptr;
        }
        break;
    case AMF_OBJECT:
    case AMF_ECMA_ARRAY:
        if (_value.object) {
            delete _value.object;
            _value.object = nullptr;
        }
        break;
    case AMF_STRICT_ARRAY:
        if (_value.array) {
            delete _value.array;
            _value.array = nullptr;
        }
        break;
    default:
        break;
    }
}

inline void AMFValue::init() {
    switch (_type) {
    case AMF_OBJECT:
    case AMF_ECMA_ARRAY:
        _value.object = new mapType;
        break;
    case AMF_STRING:
        _value.string = new std::string;
        break;
    case AMF_STRICT_ARRAY:
        _value.array = new arrayType;
        break;

    default:
        break;
    }
}

AMFValue::AMFValue(AMFType type) :
        _type(type) {
    init();
}

AMFValue::~AMFValue() {
    destroy();
}

AMFValue::AMFValue(const char *s) :
        _type(AMF_STRING) {
    init();
    *_value.string = s;
}

AMFValue::AMFValue(const std::string &s) :
        _type(AMF_STRING) {
    init();
    *_value.string = s;
}

AMFValue::AMFValue(double n) :
        _type(AMF_NUMBER) {
    init();
    _value.number = n;
}

AMFValue::AMFValue(int i) :
        _type(AMF_INTEGER) {
    init();
    _value.integer = i;
}

AMFValue::AMFValue(bool b) :
        _type(AMF_BOOLEAN) {
    init();
    _value.boolean = b;
}

AMFValue::AMFValue(const AMFValue &from) :
        _type(AMF_NULL) {
    *this = from;
}

AMFValue& AMFValue::operator = (const AMFValue &from) {
    destroy();
    _type = from._type;
    init();
    switch (_type) {
    case AMF_STRING:
        *_value.string = (*from._value.string);
        break;
    case AMF_OBJECT:
    case AMF_ECMA_ARRAY:
        *_value.object = (*from._value.object);
        break;
    case AMF_STRICT_ARRAY:
        *_value.array = (*from._value.array);
        break;
    case AMF_NUMBER:
        _value.number = from._value.number;
        break;
    case AMF_INTEGER:
        _value.integer = from._value.integer;
        break;
    case AMF_BOOLEAN:
        _value.boolean = from._value.boolean;
        break;
    default:
        break;
    }
    return *this;
}

void AMFValue::clear() {
    switch (_type) {
        case AMF_STRING:
            _value.string->clear();
            break;
        case AMF_OBJECT:
        case AMF_ECMA_ARRAY:
            _value.object->clear();
            break;
        default:
            break;
    }
}

AMFType AMFValue::type() const {
    return _type;
}

const std::string &AMFValue::as_string() const {
    if(_type != AMF_STRING){
        throw std::runtime_error("AMF not a string");
    }
    return *_value.string;
}

double AMFValue::as_number() const {
    switch (_type) {
        case AMF_NUMBER:
            return _value.number;
        case AMF_INTEGER:
            return _value.integer;
        case AMF_BOOLEAN:
            return _value.boolean;
        default:
            throw std::runtime_error("AMF not a number");
    }
}

int AMFValue::as_integer() const {
    switch (_type) {
        case AMF_NUMBER:
            return _value.number;
        case AMF_INTEGER:
            return _value.integer;
        case AMF_BOOLEAN:
            return _value.boolean;
        default:
            throw std::runtime_error("AMF not a integer");
    }
}

bool AMFValue::as_boolean() const {
    switch (_type) {
        case AMF_NUMBER:
            return _value.number;
        case AMF_INTEGER:
            return _value.integer;
        case AMF_BOOLEAN:
            return _value.boolean;
        default:
            throw std::runtime_error("AMF not a boolean");
    }
}

string AMFValue::to_string() const{
    switch (_type) {
        case AMF_NUMBER:
            return StrPrinter << _value.number;
        case AMF_INTEGER:
            return StrPrinter << _value.integer;
        case AMF_BOOLEAN:
            return _value.boolean ? "true" : "false";
        case AMF_STRING:
            return *(_value.string);
        case AMF_OBJECT:
            return "object";
        case AMF_NULL:
            return "null";
        case AMF_UNDEFINED:
            return "undefined";
        case AMF_ECMA_ARRAY:
            return "ecma_array";
        case AMF_STRICT_ARRAY:
            return "strict_array";
        default:
            throw std::runtime_error("can not convert to string ");
    }
}

const AMFValue& AMFValue::operator[](const char *str) const {
    if (_type != AMF_OBJECT && _type != AMF_ECMA_ARRAY) {
        throw std::runtime_error("AMF not a object");
    }
    auto i = _value.object->find(str);
    if (i == _value.object->end()) {
        static AMFValue val(AMF_NULL);
        return val;
    }
    return i->second;
}

void AMFValue::object_for_each(const function<void(const string &key, const AMFValue &val)> &fun) const {
    if (_type != AMF_OBJECT && _type != AMF_ECMA_ARRAY) {
        throw std::runtime_error("AMF not a object");
    }
    for (auto & pr : *(_value.object)) {
        fun(pr.first, pr.second);
    }
}

AMFValue::operator bool() const{
    return _type != AMF_NULL;
}
void AMFValue::set(const std::string &s, const AMFValue &val) {
    if (_type != AMF_OBJECT && _type != AMF_ECMA_ARRAY) {
        throw std::runtime_error("AMF not a object");
    }
    _value.object->emplace(s, val);
}
void AMFValue::add(const AMFValue &val) {
    if (_type != AMF_STRICT_ARRAY) {
        throw std::runtime_error("AMF not a array");
    }
    assert(_type == AMF_STRICT_ARRAY);
    _value.array->push_back(val);
}

const AMFValue::mapType &AMFValue::getMap() const {
    if (_type != AMF_OBJECT && _type != AMF_ECMA_ARRAY) {
        throw std::runtime_error("AMF not a object");
    }
    return *_value.object;
}
const AMFValue::arrayType &AMFValue::getArr() const {
    if (_type != AMF_STRICT_ARRAY) {
        throw std::runtime_error("AMF not a array");
    }
    return *_value.array;
}
///////////////////////////////////////////////////////////////////////////

enum {
    AMF0_NUMBER,
    AMF0_BOOLEAN,
    AMF0_STRING,
    AMF0_OBJECT,
    AMF0_MOVIECLIP,
    AMF0_NULL,
    AMF0_UNDEFINED,
    AMF0_REFERENCE,
    AMF0_ECMA_ARRAY,
    AMF0_OBJECT_END,
    AMF0_STRICT_ARRAY,
    AMF0_DATE,
    AMF0_LONG_STRING,
    AMF0_UNSUPPORTED,
    AMF0_RECORD_SET,
    AMF0_XML_OBJECT,
    AMF0_TYPED_OBJECT,
    AMF0_SWITCH_AMF3,
};

enum {
    AMF3_UNDEFINED,
    AMF3_NULL,
    AMF3_FALSE,
    AMF3_TRUE,
    AMF3_INTEGER,
    AMF3_NUMBER,
    AMF3_STRING,
    AMF3_LEGACY_XML,
    AMF3_DATE,
    AMF3_ARRAY,
    AMF3_OBJECT,
    AMF3_XML,
    AMF3_BYTE_ARRAY,
};

////////////////////////////////Encoder//////////////////////////////////////////
AMFEncoder & AMFEncoder::operator <<(const char *s) {
    if (s) {
        buf += char(AMF0_STRING);
        uint16_t str_len = htons(strlen(s));
        buf.append((char *) &str_len, 2);
        buf += s;
    } else {
        buf += char(AMF0_NULL);
    }
    return *this;
}

AMFEncoder & AMFEncoder::operator <<(const std::string &s) {
    if (!s.empty()) {
        buf += char(AMF0_STRING);
        uint16_t str_len = htons(s.size());
        buf.append((char *) &str_len, 2);
        buf += s;
    } else {
        buf += char(AMF0_NULL);
    }
    return *this;
}

AMFEncoder & AMFEncoder::operator <<(std::nullptr_t) {
    buf += char(AMF0_NULL);
    return *this;
}

AMFEncoder & AMFEncoder::write_undefined() {
    buf += char(AMF0_UNDEFINED);
    return *this;

}

AMFEncoder & AMFEncoder::operator <<(const int n){
    return (*this) << (double)n;
}

AMFEncoder & AMFEncoder::operator <<(const double n) {
    buf += char(AMF0_NUMBER);
    uint64_t encoded = 0;
    memcpy(&encoded, &n, 8);
    uint32_t val = htonl(encoded >> 32);
    buf.append((char *) &val, 4);
    val = htonl(encoded);
    buf.append((char *) &val, 4);
    return *this;
}

AMFEncoder & AMFEncoder::operator <<(const bool b) {
    buf += char(AMF0_BOOLEAN);
    buf += char(b);
    return *this;
}

AMFEncoder & AMFEncoder::operator <<(const AMFValue& value) {
    switch ((int) value.type()) {
    case AMF_STRING:
        *this << value.as_string();
        break;
    case AMF_NUMBER:
        *this << value.as_number();
        break;
    case AMF_INTEGER:
        *this << value.as_integer();
        break;
    case AMF_BOOLEAN:
        *this << value.as_boolean();
        break;
    case AMF_OBJECT: {
        buf += char(AMF0_OBJECT);
        for (auto &pr : value.getMap()) {
            write_key(pr.first);
            *this << pr.second;
        }
        write_key("");
        buf += char(AMF0_OBJECT_END);
    }
        break;
    case AMF_ECMA_ARRAY: {
        buf += char(AMF0_ECMA_ARRAY);
        uint32_t sz = htonl(value.getMap().size());
        buf.append((char *) &sz, 4);
        for (auto &pr : value.getMap()) {
            write_key(pr.first);
            *this << pr.second;
        }
        write_key("");
        buf += char(AMF0_OBJECT_END);
    }
        break;
    case AMF_NULL:
        *this << nullptr;
        break;
    case AMF_UNDEFINED:
        this->write_undefined();
        break;
    case AMF_STRICT_ARRAY: {
        buf += char(AMF0_STRICT_ARRAY);
        uint32_t sz = htonl(value.getArr().size());
        buf.append((char *) &sz, 4);
        for (auto &val : value.getArr()) {
            *this << val;
        }
        //write_key("");
        //buf += char(AMF0_OBJECT_END);
    }
        break;
    }
    return *this;

}

void AMFEncoder::write_key(const std::string& s) {
    uint16_t str_len = htons(s.size());
    buf.append((char *) &str_len, 2);
    buf += s;
}

void AMFEncoder::clear() {
    buf.clear();
}

const std::string& AMFEncoder::data() const {
    return buf;
}

//////////////////Decoder//////////////////

uint8_t AMFDecoder::front() {
    if (pos >= buf.size()) {
        throw std::runtime_error("Not enough data");
    }
    return uint8_t(buf[pos]);
}

uint8_t AMFDecoder::pop_front() {
    if (version == 0 && front() == AMF0_SWITCH_AMF3) {
        InfoL << "entering AMF3 mode";
        pos++;
        version = 3;
    }

    if (pos >= buf.size()) {
        throw std::runtime_error("Not enough data");
    }
    return uint8_t(buf[pos++]);
}

template<>
double AMFDecoder::load<double>() {
    if (pop_front() != AMF0_NUMBER) {
        throw std::runtime_error("Expected a number");
    }
    if (pos + 8 > buf.size()) {
        throw std::runtime_error("Not enough data");
    }
    uint64_t val = ((uint64_t) load_be32(&buf[pos]) << 32)
            | load_be32(&buf[pos + 4]);
    double n = 0;
    memcpy(&n, &val, 8);
    pos += 8;
    return n;

}

template<>
bool AMFDecoder::load<bool>() {
    if (pop_front() != AMF0_BOOLEAN) {
        throw std::runtime_error("Expected a boolean");
    }
    return pop_front() != 0;
}
template<>
unsigned int AMFDecoder::load<unsigned int>() {
    unsigned int value = 0;
    for (int i = 0; i < 4; ++i) {
        uint8_t b = pop_front();
        if (i == 3) {
            /* use all bits from 4th byte */
            value = (value << 8) | b;
            break;
        }
        value = (value << 7) | (b & 0x7f);
        if ((b & 0x80) == 0)
            break;
    }
    return value;
}

template<>
int AMFDecoder::load<int>() {
    if (version == 3) {
        return load<unsigned int>();
    } else {
        return load<double>();
    }
}

template<>
std::string AMFDecoder::load<std::string>() {
    size_t str_len = 0;
    uint8_t type = pop_front();
    if (version == 3) {
        if (type != AMF3_STRING) {
            throw std::runtime_error("Expected a string");
        }
        str_len = load<unsigned int>() / 2;

    } else {
        if (type != AMF0_STRING) {
            throw std::runtime_error("Expected a string");
        }
        if (pos + 2 > buf.size()) {
            throw std::runtime_error("Not enough data");
        }
        str_len = load_be16(&buf[pos]);
        pos += 2;
    }
    if (pos + str_len > buf.size()) {
        throw std::runtime_error("Not enough data");
    }
    std::string s(buf, pos, str_len);
    pos += str_len;
    return s;
}

template<>
AMFValue AMFDecoder::load<AMFValue>() {
    uint8_t type = front();
    if (version == 3) {
        switch (type) {
        case AMF3_STRING:
            return load<std::string>();
        case AMF3_NUMBER:
            return load<double>();
        case AMF3_INTEGER:
            return load<int>();
        case AMF3_FALSE:
            pos++;
            return false;
        case AMF3_TRUE:
            pos++;
            return true;
        case AMF3_OBJECT:
            return load_object();
        case AMF3_ARRAY:
            return load_ecma();
        case AMF3_NULL:
            pos++;
            return AMF_NULL;
        case AMF3_UNDEFINED:
            pos++;
            return AMF_UNDEFINED;
        default:
            throw std::runtime_error(
            StrPrinter << "Unsupported AMF3 type:" << (int) type << endl);
        }
    } else {
        switch (type) {
        case AMF0_STRING:
            return load<std::string>();
        case AMF0_NUMBER:
            return load<double>();
        case AMF0_BOOLEAN:
            return load<bool>();
        case AMF0_OBJECT:
            return load_object();
        case AMF0_ECMA_ARRAY:
            return load_ecma();
        case AMF0_NULL:
            pos++;
            return AMF_NULL;
        case AMF0_UNDEFINED:
            pos++;
            return AMF_UNDEFINED;
        case AMF0_STRICT_ARRAY:
            return load_arr();
        default:
            throw std::runtime_error(
            StrPrinter << "Unsupported AMF type:" << (int) type << endl);
        }
    }

}

std::string AMFDecoder::load_key() {
    if (pos + 2 > buf.size()) {
        throw std::runtime_error("Not enough data");
    }
    size_t str_len = load_be16(&buf[pos]);
    pos += 2;
    if (pos + str_len > buf.size()) {
        throw std::runtime_error("Not enough data");
    }
    std::string s(buf, pos, str_len);
    pos += str_len;
    return s;

}

AMFValue AMFDecoder::load_object() {
    AMFValue object(AMF_OBJECT);
    if (pop_front() != AMF0_OBJECT) {
        throw std::runtime_error("Expected an object");
    }
    while (1) {
        std::string key = load_key();
        if (key.empty())
            break;
        AMFValue value = load<AMFValue>();
        object.set(key, value);
    }
    if (pop_front() != AMF0_OBJECT_END) {
        throw std::runtime_error("expected object end");
    }
    return object;
}

AMFValue AMFDecoder::load_ecma() {
    /* ECMA array is the same as object, with 4 extra zero bytes */
    AMFValue object(AMF_ECMA_ARRAY);
    if (pop_front() != AMF0_ECMA_ARRAY) {
        throw std::runtime_error("Expected an ECMA array");
    }
    if (pos + 4 > buf.size()) {
        throw std::runtime_error("Not enough data");
    }
    pos += 4;
    while (1) {
        std::string key = load_key();
        if (key.empty())
            break;
        AMFValue value = load<AMFValue>();
        object.set(key, value);
    }
    if (pop_front() != AMF0_OBJECT_END) {
        throw std::runtime_error("expected object end");
    }
    return object;
}
AMFValue AMFDecoder::load_arr() {
    /* ECMA array is the same as object, with 4 extra zero bytes */
    AMFValue object(AMF_STRICT_ARRAY);
    if (pop_front() != AMF0_STRICT_ARRAY) {
        throw std::runtime_error("Expected an STRICT array");
    }
    if (pos + 4 > buf.size()) {
        throw std::runtime_error("Not enough data");
    }
    int arrSize = load_be32(&buf[pos]);
    pos += 4;
    while (arrSize--) {
        AMFValue value = load<AMFValue>();
        object.add(value);
    }
    /*pos += 2;
    if (pop_front() != AMF0_OBJECT_END) {
        throw std::runtime_error("expected object end");
    }*/
    return object;
}

AMFDecoder::AMFDecoder(const std::string &buf_in, size_t pos_in, int version_in) :
        buf(buf_in), pos(pos_in), version(version_in) {
}

