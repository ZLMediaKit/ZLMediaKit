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
#include "strCoding.h"

namespace ZL {
namespace Http {

inline char strCoding::CharToInt(char ch) {
	if (ch >= '0' && ch <= '9')
		return (char) (ch - '0');
	if (ch >= 'a' && ch <= 'f')
		return (char) (ch - 'a' + 10);
	if (ch >= 'A' && ch <= 'F')
		return (char) (ch - 'A' + 10);
	return -1;
}
inline char strCoding::StrToBin(const char *str) {
	char tempWord[2];
	char chn;
	tempWord[0] = CharToInt(str[0]); //make the B to 11 -- 00001011
	tempWord[1] = CharToInt(str[1]); //make the 0 to 0 -- 00000000
	chn = (tempWord[0] << 4) | tempWord[1]; //to change the BO to 10110000
	return chn;
}
string strCoding::UrlUTF8Encode(const char * str) {
	string dd;
	size_t len = strlen(str);
	for (size_t i = 0; i < len; i++) {
		if (isalnum((uint8_t) str[i])) {
			char tempbuff[2];
			sprintf(tempbuff, "%c", str[i]);
			dd.append(tempbuff);
		} else if (isspace((uint8_t) str[i])) {
			dd.append("+");
		} else {
			char tempbuff[4];
			sprintf(tempbuff, "%%%X%X", ((uint8_t*) str)[i] >> 4,
					((uint8_t*) str)[i] % 16);
			dd.append(tempbuff);
		}
	}
	return dd;
}
string strCoding::UrlUTF8Decode(const string &str) {
	string output = "";
	char tmp[2];
	int i = 0,  len = str.length();
	while (i < len) {
		if (str[i] == '%') {
			tmp[0] = str[i + 1];
			tmp[1] = str[i + 2];
			output += StrToBin(tmp);
			i = i + 3;
		} else if (str[i] == '+') {
			output += ' ';
			i++;
		} else {
			output += str[i];
			i++;
		}
	}
	return output;
}

} /* namespace Http */
} /* namespace ZL */
