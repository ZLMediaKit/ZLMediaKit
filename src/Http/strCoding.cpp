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

#if defined(_WIN32)
#include <windows.h> 
#endif//defined(_WIN32)

namespace ZL {
namespace Http {

//////////////////////////通用///////////////////////
void UTF8ToUnicode(wchar_t* pOut, const char *pText)
{
	char* uchar = (char *)pOut;
	uchar[1] = ((pText[0] & 0x0F) << 4) + ((pText[1] >> 2) & 0x0F);
	uchar[0] = ((pText[1] & 0x03) << 6) + (pText[2] & 0x3F);
	return;
}
void UnicodeToUTF8(char* pOut, const wchar_t* pText)
{
	// 注意 WCHAR高低字的顺序,低字节在前，高字节在后 
	const char* pchar = (const char *)pText;
	pOut[0] = (0xE0 | ((pchar[1] & 0xF0) >> 4));
	pOut[1] = (0x80 | ((pchar[1] & 0x0F) << 2)) + ((pchar[0] & 0xC0) >> 6);
	pOut[2] = (0x80 | (pchar[0] & 0x3F));
	return;
}

char CharToInt(char ch) 
{
	if (ch >= '0' && ch <= '9')return (char)(ch - '0');
	if (ch >= 'a' && ch <= 'f')return (char)(ch - 'a' + 10);
	if (ch >= 'A' && ch <= 'F')return (char)(ch - 'A' + 10);
	return -1;
}
char StrToBin(const char *str) 
{
	char tempWord[2];
	char chn;
	tempWord[0] = CharToInt(str[0]); //make the B to 11 -- 00001011 
	tempWord[1] = CharToInt(str[1]); //make the 0 to 0 -- 00000000 
	chn = (tempWord[0] << 4) | tempWord[1]; //to change the BO to 10110000 
	return chn;
}

string strCoding::UrlUTF8Encode(const string &str) {
	string dd;
	size_t len = str.size();
	for (size_t i = 0; i < len; i++) {
		if (isalnum((uint8_t)str[i])) {
			char tempbuff[2];
			sprintf(tempbuff, "%c", str[i]);
			dd.append(tempbuff);
		}
		else if (isspace((uint8_t)str[i])) {
			dd.append("+");
		}
		else {
			char tempbuff[4];
			sprintf(tempbuff, "%%%X%X", (uint8_t)str[i] >> 4,(uint8_t)str[i] % 16);
			dd.append(tempbuff);
		}
	}
	return dd;
}
string strCoding::UrlUTF8Decode(const string &str) {
	string output = "";
	char tmp[2];
	int i = 0, len = str.length();
	while (i < len) {
		if (str[i] == '%') {
			tmp[0] = str[i + 1];
			tmp[1] = str[i + 2];
			output += StrToBin(tmp);
			i = i + 3;
		}
		else if (str[i] == '+') {
			output += ' ';
			i++;
		}
		else {
			output += str[i];
			i++;
		}
	}
	return output;
}

string strCoding::UrlGB2312Encode(const string &str)
{
	string dd;
	size_t len = str.size();
	for (size_t i = 0; i<len; i++)
	{
		if (isalnum((uint8_t)str[i]))
		{
			char tempbuff[2];
			sprintf(tempbuff, "%c", str[i]);
			dd.append(tempbuff);
		}
		else if (isspace((uint8_t)str[i]))
		{
			dd.append("+");
		}
		else
		{
			char tempbuff[4];
			sprintf(tempbuff, "%%%X%X", (uint8_t)str[i] >> 4, (uint8_t)str[i] % 16);
			dd.append(tempbuff);
		}
	}
	return dd;
}

string strCoding::UrlGB2312Decode(const string &str)
{
	string output = "";
	char tmp[2];
	int i = 0, idx = 0, len = str.length();
	while (i<len) {
		if (str[i] == '%') {
			tmp[0] = str[i + 1];
			tmp[1] = str[i + 2];
			output += StrToBin(tmp);
			i = i + 3;
		}
		else if (str[i] == '+') {
			output += ' ';
			i++;
		}
		else {
			output += str[i];
			i++;
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

string strCoding::UTF8ToGB2312(const string &str) {
	auto len = str.size();
	auto pText = str.data();
	char Ctemp[4] = {0};
	char *pOut = new char[len + 1];
	memset(pOut, 0, len + 1);

	int i = 0, j = 0;
	while (i < len)
	{
		if (pText[i] >= 0)
		{
			pOut[j++] = pText[i++];
		}
		else
		{
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
}

string strCoding::GB2312ToUTF8(const string &str) {
	auto len = str.size();
	auto pText = str.data();
	char buf[4] = { 0 };
	int nLength = len * 3;
	char* pOut = new char[nLength];
	memset(pOut, 0, nLength);
	int i = 0, j = 0;
	while (i < len)
	{
		//如果是英文直接复制就可以   
		if (*(pText + i) >= 0)
		{
			pOut[j++] = pText[i++];
		}
		else
		{
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
}
#endif//defined(_WIN32)




} /* namespace Http */
} /* namespace ZL */
