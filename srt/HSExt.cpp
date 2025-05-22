#include "HSExt.hpp"

namespace SRT {

bool HSExtMessage::loadFromData(uint8_t *buf, size_t len) {
    if (buf == NULL || len != HSEXT_MSG_SIZE) {
        return false;
    }

    _data = BufferRaw::create();
    _data->assign((char *)buf, len);
    extension_length = 3;
    HSExt::loadHeader();

    assert(extension_type == SRT_CMD_HSREQ || extension_type == SRT_CMD_HSRSP);

    uint8_t *ptr = (uint8_t *)_data->data() + 4;
    srt_version = loadUint32(ptr);
    ptr += 4;

    srt_flag = loadUint32(ptr);
    ptr += 4;

    recv_tsbpd_delay = loadUint16(ptr);
    ptr += 2;

    send_tsbpd_delay = loadUint16(ptr);
    ptr += 2;

    return true;
}

std::string HSExtMessage::dump() {
    _StrPrinter printer;
    printer << "srt version : " << std::hex << srt_version << " srt flag : " << std::hex << srt_flag
            << " recv_tsbpd_delay=" << recv_tsbpd_delay << " send_tsbpd_delay = " << send_tsbpd_delay;
    return std::move(printer);
}

bool HSExtMessage::storeToData() {
    _data = BufferRaw::create();
    _data->setCapacity(HSEXT_MSG_SIZE);
    _data->setSize(HSEXT_MSG_SIZE);
    extension_length = 3;
    HSExt::storeHeader();
    uint8_t *ptr = (uint8_t *)_data->data() + 4;

    storeUint32(ptr, srt_version);
    ptr += 4;

    storeUint32(ptr, srt_flag);
    ptr += 4;

    storeUint16(ptr, recv_tsbpd_delay);
    ptr += 2;

    storeUint16(ptr, send_tsbpd_delay);
    ptr += 2;
    return true;
}

bool HSExtStreamID::loadFromData(uint8_t *buf, size_t len) {
    if (buf == NULL || len < 4) {
        return false;
    }
    _data = BufferRaw::create();
    _data->assign((char *)buf, len);

    HSExt::loadHeader();

    size_t content_size = extension_length * 4;
    if (len < content_size + 4) {
        return false;
    }
    streamid.clear();
    char *ptr = _data->data() + 4;

    for (size_t i = 0; i < extension_length; ++i) {
        streamid.push_back(*(ptr + 3));
        streamid.push_back(*(ptr + 2));
        streamid.push_back(*(ptr + 1));
        streamid.push_back(*(ptr));
        ptr += 4;
    }
    char zero = 0x00;
    if (streamid.back() == zero) {
        streamid.erase(streamid.find_first_of(zero), streamid.size());
    }
    return true;
}

bool HSExtStreamID::storeToData() {
    size_t content_size = ((streamid.length() + 4) + 3) / 4 * 4;

    _data = BufferRaw::create();
    _data->setCapacity(content_size);
    _data->setSize(content_size);
    extension_length = (content_size - 4) / 4;
    extension_type = SRT_CMD_SID;
    HSExt::storeHeader();
    auto ptr = _data->data() + 4;
    memset(ptr, 0, extension_length * 4);
    const char *src = streamid.c_str();
    for (size_t i = 0; i < streamid.length() / 4; ++i) {
        *ptr = *(src + 3 + i * 4);
        ptr++;

        *ptr = *(src + 2 + i * 4);
        ptr++;

        *ptr = *(src + 1 + i * 4);
        ptr++;

        *ptr = *(src + 0 + i * 4);
        ptr++;
    }

    ptr += 3;
    size_t offset = streamid.length() / 4 * 4;
    for (size_t i = 0; i < streamid.length() % 4; ++i) {
        *ptr = *(src + offset + i);
        ptr -= 1;
    }

    return true;
}

std::string HSExtStreamID::dump() {
    _StrPrinter printer;
    printer << " streamid : " << streamid;
    return std::move(printer);
}

size_t KeyMaterial::getContentSize() {
    size_t variable_width = _slen + _warpped_key.size();
    size_t content_size = variable_width + 16;
    return content_size;
}

bool KeyMaterial::loadFromData(uint8_t *buf, size_t len) {
    if (buf == NULL || len < 16) {
        return false;
    }
    uint8_t *ptr = (uint8_t *)buf;

    _km_version = (*ptr & 0x70) >> 4;
    _pt = *ptr & 0x0f;
    ptr += 1;

    _sign = loadUint16(ptr);
    ptr += 2;

    _kk = *ptr & 0x03;
    auto sek_num = 1;
    if (_kk == KEY_BASED_ENCRYPTION_BOTH_SEK) {
        sek_num = 2;
    }
    ptr += 1;

    _keki = loadUint32(ptr);
    ptr += 4;

    _cipher = *ptr;
    ptr += 1;

    _auth = *ptr;
    ptr += 1;

    _se = *ptr;
    ptr += 1;

    //Resv2
    ptr += 1;
    //Resv3
    ptr += 2;

    _slen = *ptr *4;
    ptr += 1;

    _klen = *ptr *4;
    ptr += 1;

    size_t wrapped_key_len = 8 + sek_num * _klen;
    size_t variable_width = _slen + wrapped_key_len;
    if (len < variable_width + 16) {
        return false;
    }

    _salt.assign((const char*)ptr, (size_t)_slen);
    ptr += _slen;

    _warpped_key.assign((const char*)ptr, (size_t)wrapped_key_len);

    return true;
}

bool KeyMaterial::storeToData(uint8_t *buf, size_t len) {
    auto content_size = getContentSize();
    if (len < content_size) {
        return false;
    }

    uint8_t *ptr = (uint8_t *)buf;
    memset(ptr, 0, len);

    *ptr = ((_km_version << 4)& 0x70) | (_pt & 0x0f);
    ptr += 1;

    storeUint16(ptr, _sign);
    ptr += 2;

    *ptr = _kk & 0x03;
    ptr += 1;

    storeUint32(ptr, _keki);
    ptr += 4;

    *ptr = _cipher;
    ptr += 1;

    *ptr = _auth;
    ptr += 1;

    *ptr = _se;
    ptr += 1;

    *ptr = 0; //Resv2
    ptr += 1;

    storeUint16(ptr, 0);//Resv3
    ptr += 2;

    *ptr = (uint8_t)(_slen/4);
    ptr += 1;

    *ptr = (uint8_t)(_klen/4);
    ptr += 1;

    const char *src = _salt.data();
    for (size_t i = 0; i < _salt.size(); ptr++, src++, i++) {
        *ptr = *src;
    }

    src = _warpped_key.data();
    for (size_t i = 0; i < _warpped_key.size(); ptr++, src++, i++) {
        *ptr = *src;
    }
    return true;
}

std::string KeyMaterial::dump() {
    _StrPrinter printer;
    printer << "kmVersion: " << _km_version 
        << " pt : " << _pt 
        << " sign : " << std::hex << _sign
        << " kk : " << _kk 
        << " keki : " << _keki 
        << " cipher : " << _cipher 
        << " auth : " << _auth 
        << " se : " << _se 
        << " sLen : " << _slen 
        << " salt : " << std::hex << _salt.data()
        << " kLen : " << _klen;
    return std::move(printer);
}

bool HSExtKeyMaterial::loadFromData(uint8_t *buf, size_t len) {
    if (buf == NULL || len < 4) {
        return false;
    }
    HSExt::_data = BufferRaw::create();
    HSExt::_data->assign((char *)buf, len);
    HSExt::loadHeader();
    assert(extension_type == SRT_CMD_KMREQ || extension_type == SRT_CMD_KMRSP);
    return KeyMaterial::loadFromData(buf +4, len -4);
}

bool HSExtKeyMaterial::storeToData() {
    size_t content_size = ((KeyMaterial::getContentSize() + 4) + 3) / 4 * 4;
    HSExt::_data = BufferRaw::create();
    HSExt::_data->setCapacity(content_size);
    HSExt::_data->setSize(content_size);
    extension_length = (content_size - 4) / 4;
    HSExt::storeHeader();
    return KeyMaterial::storeToData((uint8_t*)_data->data() + 4, content_size - 4);
}

std::string HSExtKeyMaterial::dump() {
    return KeyMaterial::dump();
}

} // namespace SRT
