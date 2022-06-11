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

} // namespace SRT