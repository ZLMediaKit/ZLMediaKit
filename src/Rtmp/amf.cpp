/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <string.h>
#include <stdexcept>
#include "amf.h"
#include "utils.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Network/sockutil.h"

using namespace ZL::Util;
using namespace ZL::Network;

/////////////////////AMFValue/////////////////////////////
inline void AMFValue::destroy() {
	switch (m_type) {
	case AMF_STRING:
		if (m_value.string) {
			delete m_value.string;
			m_value.string = nullptr;
		}
		break;
	case AMF_OBJECT:
	case AMF_ECMA_ARRAY:
		if (m_value.object) {
			delete m_value.object;
			m_value.object = nullptr;
		}
		break;
	case AMF_STRICT_ARRAY:
		if (m_value.array) {
			delete m_value.array;
			m_value.array = nullptr;
		}
		break;
	default:
		break;
	}
}
inline void AMFValue::init() {
	switch (m_type) {
	case AMF_OBJECT:
	case AMF_ECMA_ARRAY:
		m_value.object = new mapType;
		break;
	case AMF_STRING:
		m_value.string = new std::string;
		break;
	case AMF_STRICT_ARRAY:
		m_value.array = new arrayType;
		break;

	default:
		break;
	}

}
AMFValue::AMFValue(AMFType type) :
		m_type(type) {
	init();
}


AMFValue::~AMFValue() {
	destroy();
}

AMFValue::AMFValue(const char *s) :
		m_type(AMF_STRING) {
	init();
	*m_value.string = s;
}


AMFValue::AMFValue(const std::string &s) :
		m_type(AMF_STRING) {
	init();
	*m_value.string = s;
}

AMFValue::AMFValue(double n) :
		m_type(AMF_NUMBER) {
	init();
	m_value.number = n;
}

AMFValue::AMFValue(int i) :
		m_type(AMF_INTEGER) {
	init();
	m_value.integer = i;
}

AMFValue::AMFValue(bool b) :
		m_type(AMF_BOOLEAN) {
	init();
	m_value.boolean = b;
}

AMFValue::AMFValue(const AMFValue &from) :
		m_type(AMF_NULL) {
	*this = from;
}

AMFValue::AMFValue(AMFValue &&from) {
	*this = std::forward<AMFValue>(from);
}

AMFValue& AMFValue::operator =(const AMFValue &from) {
	return *this = const_cast<AMFValue &&>(from);

}
AMFValue& AMFValue::operator =(AMFValue &&from) {
	destroy();
	m_type = from.m_type;
	init();
	switch (m_type) {
	case AMF_STRING:
		*m_value.string = (*from.m_value.string);
		break;
	case AMF_OBJECT:
	case AMF_ECMA_ARRAY:
		*m_value.object = (*from.m_value.object);
		break;
	case AMF_STRICT_ARRAY:
		*m_value.array = (*from.m_value.array);
		break;
	case AMF_NUMBER:
		m_value.number = from.m_value.number;
		break;
	case AMF_INTEGER:
		m_value.integer = from.m_value.integer;
		break;
	case AMF_BOOLEAN:
		m_value.boolean = from.m_value.boolean;
		break;
	default:
		break;
	}
	return *this;

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
