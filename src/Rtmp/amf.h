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
#ifndef __amf_h
#define __amf_h

#include <assert.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
enum AMFType {
	AMF_NUMBER,
	AMF_INTEGER,
	AMF_BOOLEAN,
	AMF_STRING,
	AMF_OBJECT,
	AMF_NULL,
	AMF_UNDEFINED,
	AMF_ECMA_ARRAY,
	AMF_STRICT_ARRAY,
};

class AMFValue;

class AMFValue {
public:

	AMFValue(AMFType type = AMF_NULL);
	AMFValue(const char *s);
	AMFValue(const std::string &s);
	AMFValue(double n);
	AMFValue(int i);
	AMFValue(bool b);
	AMFValue(const AMFValue &from);
	AMFValue(AMFValue &&from);
	AMFValue &operator =(const AMFValue &from);
	AMFValue &operator =(AMFValue &&from);
	~AMFValue();

	void clear() {
		switch (m_type) {
		case AMF_STRING:
			m_value.string->clear();
			break;
		case AMF_OBJECT:
		case AMF_ECMA_ARRAY:
			m_value.object->clear();
			break;
		default:
			break;
		}
	}

	AMFType type() const {
		return m_type;
	}

	const std::string &as_string() const {
		if(m_type != AMF_STRING){
			throw std::runtime_error("AMF not a string");
		}
		return *m_value.string;
	}
	double as_number() const {
		switch (m_type) {
		case AMF_NUMBER:
			return m_value.number;
		case AMF_INTEGER:
			return m_value.integer;
		case AMF_BOOLEAN:
			return m_value.boolean;
			break;
		default:
			throw std::runtime_error("AMF not a number");
			break;
		}
	}
	int as_integer() const {
		switch (m_type) {
		case AMF_NUMBER:
			return m_value.number;
		case AMF_INTEGER:
			return m_value.integer;
		case AMF_BOOLEAN:
			return m_value.boolean;
			break;
		default:
			throw std::runtime_error("AMF not a integer");
			break;
		}
	}
	bool as_boolean() const {
		switch (m_type) {
		case AMF_NUMBER:
			return m_value.number;
		case AMF_INTEGER:
			return m_value.integer;
		case AMF_BOOLEAN:
			return m_value.boolean;
			break;
		default:
			throw std::runtime_error("AMF not a boolean");
			break;
		}
	}

	const AMFValue &operator[](const char *str) const {
		if (m_type != AMF_OBJECT && m_type != AMF_ECMA_ARRAY) {
			throw std::runtime_error("AMF not a object");
		}
		auto i = m_value.object->find(str);
		if (i == m_value.object->end()) {
			static AMFValue val(AMF_NULL);
			return val;
		}
		return i->second;
	}
	template<typename FUN>
	void object_for_each(const FUN &fun) const {
		if (m_type != AMF_OBJECT && m_type != AMF_ECMA_ARRAY) {
			throw std::runtime_error("AMF not a object");
		}
		for (auto & pr : *(m_value.object)) {
			fun(pr.first, pr.second);
		}
	}

	operator bool() const{
		return m_type != AMF_NULL;
	}
	void set(const std::string &s, const AMFValue &val) {
		if (m_type != AMF_OBJECT && m_type != AMF_ECMA_ARRAY) {
			throw std::runtime_error("AMF not a object");
		}
		m_value.object->emplace(s, val);
	}
	void add(const AMFValue &val) {
		if (m_type != AMF_STRICT_ARRAY) {
			throw std::runtime_error("AMF not a array");
		}
		assert(m_type == AMF_STRICT_ARRAY);
		m_value.array->push_back(val);
	}

private:
	typedef std::map<std::string, AMFValue> mapType;
	typedef std::vector<AMFValue> arrayType;

	AMFType m_type;
	union {
		std::string *string;
		double number;
		int integer;
		bool boolean;
		mapType *object;
		arrayType *array;
	} m_value;

	friend class AMFEncoder;
	const mapType &getMap() const {
		if (m_type != AMF_OBJECT && m_type != AMF_ECMA_ARRAY) {
			throw std::runtime_error("AMF not a object");
		}
		return *m_value.object;
	}
	const arrayType &getArr() const {
		if (m_type != AMF_STRICT_ARRAY) {
			throw std::runtime_error("AMF not a array");
		}
		return *m_value.array;
	}
	inline void destroy();
	inline void init();
};

class AMFDecoder {
public:
	AMFDecoder(const std::string &_buf, size_t _pos, int _version = 0) :
			buf(_buf), pos(_pos), version(_version) {
	}

	int getVersion() const {
		return version;
	}

	template<typename TP>
	TP load();



private:
	const std::string &buf;
	size_t pos;
	int version;

	std::string load_key();
	AMFValue load_object();
	AMFValue load_ecma();
	AMFValue load_arr();
	uint8_t front();
	uint8_t pop_front();
};

class AMFEncoder {
public:
	AMFEncoder & operator <<(const char *s);
	AMFEncoder & operator <<(const std::string &s);
	AMFEncoder & operator <<(std::nullptr_t);
	AMFEncoder & operator <<(const int n);
	AMFEncoder & operator <<(const double n);
	AMFEncoder & operator <<(const bool b);
	AMFEncoder & operator <<(const AMFValue &value);
	const std::string data() const {
		return buf;
	}
	void clear() {
		buf.clear();
	}
private:
	void write_key(const std::string &s);
	AMFEncoder &write_undefined();
	std::string buf;
};


#endif
