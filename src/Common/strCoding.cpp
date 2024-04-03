/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>
#include "strCoding.h"

#if defined(_WIN32)
#include <windows.h> 
#endif//defined(_WIN32)

using namespace std;

namespace mediakit {

//////////////////////////通用///////////////////////
void UTF8ToUnicode(wchar_t *pOut, const char *pText) {
    char *uchar = (char *) pOut;
    uchar[1] = ((pText[0] & 0x0F) << 4) + ((pText[1] >> 2) & 0x0F);
    uchar[0] = ((pText[1] & 0x03) << 6) + (pText[2] & 0x3F);
    return;
}

void UnicodeToUTF8(char *pOut, const wchar_t *pText) {
    // 注意 WCHAR高低字的顺序,低字节在前，高字节在后 
    const char *pchar = (const char *) pText;
    pOut[0] = (0xE0 | ((pchar[1] & 0xF0) >> 4));
    pOut[1] = (0x80 | ((pchar[1] & 0x0F) << 2)) + ((pchar[0] & 0xC0) >> 6);
    pOut[2] = (0x80 | (pchar[0] & 0x3F));
    return;
}

char HexCharToBin(char ch) {
    if (ch >= '0' && ch <= '9') return (char)(ch - '0');
    if (ch >= 'a' && ch <= 'f') return (char)(ch - 'a' + 10);
    if (ch >= 'A' && ch <= 'F') return (char)(ch - 'A' + 10);
    return -1;
}

char HexStrToBin(const char *str) {
    auto high = HexCharToBin(str[0]);
    auto low = HexCharToBin(str[1]);
    if (high == -1 || low == -1) {
        // 无法把16进制字符串转换为二进制
        return -1;
    }
    return (high << 4) | low;
}

string strCoding::UrlEncodePath(const string &str) {
    const char *dont_escape = "!#&'*+:=?@/._-$,;~()";
    string out;
    size_t len = str.size();
    for (size_t i = 0; i < len; ++i) {
        char ch = str[i];
        if (isalnum((uint8_t) ch) || strchr(dont_escape, (uint8_t) ch) != NULL) {
            out.push_back(ch);
        } else {
            char buf[4];
            snprintf(buf, 4, "%%%X%X", (uint8_t) ch >> 4, (uint8_t) ch & 0x0F);
            out.append(buf);
        }
    }
    return out;
}

string strCoding::UrlEncodeComponent(const string &str) {
    const char *dont_escape = "!'()*-._~";
    string out;
    size_t len = str.size();
    for (size_t i = 0; i < len; ++i) {
        char ch = str[i];
        if (isalnum((uint8_t) ch) || strchr(dont_escape, (uint8_t) ch) != NULL) {
            out.push_back(ch);
        } else {
            char buf[4];
            snprintf(buf, 4, "%%%X%X", (uint8_t) ch >> 4, (uint8_t) ch & 0x0F);
            out.append(buf);
        }
    }
    return out;
}

string strCoding::UrlDecodePath(const string &str) {
    const char *dont_unescape = "#$&+,/:;=?@";
    string output;
    size_t i = 0, len = str.length();
    while (i < len) {
        if (str[i] == '%') {
            if (i + 3 > len) {
                // %后面必须还有两个字节才会反转义
                output.append(str, i, len - i);
                break;
            }
            char ch = HexStrToBin(&(str[i + 1]));
            if (ch == -1 || strchr(dont_unescape, (unsigned char)ch) != NULL) {
                // %后面两个字节不是16进制字符串，转义失败；或者转义出来可能会造成url包含非path部分，比如#?，说明提交的是非法拼接的url；直接拼接3个原始字符
                output.append(str, i, 3);
            } else {
                output += ch;
            }
            i += 3;
        } else {
            output += str[i];
            ++i;
        }
    }
    return output;
}

std::string strCoding::UrlDecodeComponent(const std::string &str) {
    string output;
    size_t i = 0, len = str.length();
    while (i < len) {
        if (str[i] == '%') {
            if (i + 3 > len) {
                // %后面必须还有两个字节才会反转义
                output.append(str, i, len - i);
                break;
            }
            char ch = HexStrToBin(&(str[i + 1]));
            if (ch == -1) {
                // %后面两个字节不是16进制字符串，转义失败；直接拼接3个原始字符
                output.append(str, i, 3);
            } else {
                output += ch;
            }
            i += 3;
        } else if (str[i] == '+') {
            output += ' ';
            ++i;
        } else {
            output += str[i];
            ++i;
        }
    }
    return output;
}

///////////////////////////////windows专用///////////////////////////////////
#if defined(_WIN32)
void UnicodeToGB2312(char* pOut, wchar_t uData)
{
    WideCharToMultiByte(CP_ACP, NULL, &uData, 1, pOut, sizeof(wchar_t), NULL, NULL);
}
void Gb2312ToUnicode(wchar_t* pOut, const char *gbBuffer)
{
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, gbBuffer, 2, pOut, 1);
}
#else
#include <iconv.h>
// 将 GB2312 编码的字符串转换为 UTF-8 编码
char *gb2312_to_utf8(const char *gb2312_string) {
    size_t in_len = strlen(gb2312_string);
    size_t out_len = in_len * 4; // UTF-8 最多需要 4 倍空间

    iconv_t cd = iconv_open("UTF-8", "GBK");//GBK 是在 GB2312 的基础上进行扩展的字符集，包含了 GB2312 中的所有字符
    if (cd == (iconv_t)-1) {
        perror("iconv_open");
        return NULL;
    }

    char *inbuf = (char *)gb2312_string;
    char *outbuf = (char *)malloc(out_len + 1); // 分配足够的空间来存储转换后的字符串
    if (outbuf == NULL) {
        perror("malloc");
        iconv_close(cd);
        return NULL;
    }
    memset(outbuf, 0, out_len + 1);

    char *inptr = inbuf;
    char *outptr = outbuf;

    if (iconv(cd, &inptr, &in_len, &outptr, &out_len) == (size_t)-1) {
        perror("iconv");
        free(outbuf);
        iconv_close(cd);
        return NULL;
    }

    iconv_close(cd);

    return outbuf;
}

// 跨平台的 UTF-8 转 GB2312 编码
char *utf8_to_gb2312(const char *utf8_string) {
    char *result = NULL;
    // 非 Windows 平台使用 iconv 函数进行编码转换
    size_t in_len = strlen(utf8_string);
    size_t out_len = in_len * 4; // GB2312 最多需要 4 倍空间

    iconv_t cd = iconv_open("GBK", "UTF-8");
    if (cd == (iconv_t)-1) {
        perror("iconv_open");
        return NULL;
    }

    char *inbuf = (char *)utf8_string;
    char *outbuf = (char *)malloc(out_len + 1); // 分配足够的空间来存储转换后的字符串
    if (outbuf == NULL) {
        perror("malloc");
        iconv_close(cd);
        return NULL;
    }
    memset(outbuf, 0, out_len + 1);

    char *inptr = inbuf;
    char *outptr = outbuf;

    if (iconv(cd, &inptr, &in_len, &outptr, &out_len) == (size_t)-1) {
        perror("iconv");
        free(outbuf);
        iconv_close(cd);
        return NULL;
    }

    iconv_close(cd);

    result = outbuf;
    return result;
}
#endif//defined(_WIN32)

string strCoding::UTF8ToGB2312(const string &str) {
#ifdef WIN32
    auto len = str.size();
    auto pText = str.data();
    char Ctemp[4] = { 0 };
    char *pOut = new char[len + 1];
    memset(pOut, 0, len + 1);

    int i = 0, j = 0;
    while (i < len) {
        if (pText[i] >= 0) {
            pOut[j++] = pText[i++];
        } else {
            wchar_t Wtemp;
            UTF8ToUnicode(&Wtemp, pText + i);
            UnicodeToGB2312(Ctemp, Wtemp);
            pOut[j] = Ctemp[0];
            pOut[j + 1] = Ctemp[1];
            i += 3;
            j += 2;
        }
    }
    string ret = pOut;
    delete[] pOut;
    return ret;
#else
    char *gb2312_string = utf8_to_gb2312(str.c_str());
    if (gb2312_string == NULL) {
        return "";
    }
    string result(gb2312_string);
    free(gb2312_string);
    return result;
#endif
}

string strCoding::GB2312ToUTF8(const string &str) {
#ifdef WIN32
    auto len = str.size();
    auto pText = str.data();
    char buf[4] = { 0 };
    auto nLength = len * 3;
    char *pOut = new char[nLength];
    memset(pOut, 0, nLength);
    size_t i = 0, j = 0;
    while (i < len) {
        // 如果是英文直接复制就可以
        if (*(pText + i) >= 0) {
            pOut[j++] = pText[i++];
        } else {
            wchar_t pbuffer;
            Gb2312ToUnicode(&pbuffer, pText + i);
            UnicodeToUTF8(buf, &pbuffer);
            pOut[j] = buf[0];
            pOut[j + 1] = buf[1];
            pOut[j + 2] = buf[2];
            j += 3;
            i += 2;
        }
    }
    string ret = pOut;
    delete[] pOut;
    return ret;
#else
    char *utf8_string = gb2312_to_utf8(str.c_str());
    if (utf8_string == NULL) {
        return "";
    }
    string result(utf8_string);
    free(utf8_string);
    return result;
#endif
}

} /* namespace mediakit */
