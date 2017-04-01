/*
 * strCoding.cpp
 *
 *  Created on: 2016年9月22日
 *      Author: xzl
 */

#include "strCoding.h"
#include <string.h>

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
