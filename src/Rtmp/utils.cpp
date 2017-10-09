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
#include "utils.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "Util/util.h"
#include "Network/sockutil.h"

using namespace ZL::Util;
using namespace ZL::Network;

/*
 * Used to do unaligned loads on archs that don't support them. GCC can mostly
 * optimize these away.
 */
uint32_t load_be32(const void *p)
{
	uint32_t val;
	memcpy(&val, p, sizeof val);
	return ntohl(val);
}

uint16_t load_be16(const void *p)
{
	uint16_t val;
	memcpy(&val, p, sizeof val);
	return ntohs(val);
}

uint32_t load_le32(const void *p)
{
	const uint8_t *data = (const uint8_t *) p;
	return data[0] | ((uint32_t) data[1] << 8) |
		((uint32_t) data[2] << 16) | ((uint32_t) data[3] << 24);
}

uint32_t load_be24(const void *p)
{
	const uint8_t *data = (const uint8_t *) p;
	return data[2] | ((uint32_t) data[1] << 8) | ((uint32_t) data[0] << 16);
}

void set_be24(void *p, uint32_t val)
{
	uint8_t *data = (uint8_t *) p;
	data[0] = val >> 16;
	data[1] = val >> 8;
	data[2] = val;
}

void set_le32(void *p, uint32_t val)
{
	uint8_t *data = (uint8_t *) p;
	data[0] = val;
	data[1] = val >> 8;
	data[2] = val >> 16;
	data[3] = val >> 24;
}

void set_be32(void *p, uint32_t val)
{
	uint8_t *data = (uint8_t *) p;
	data[3] = val;
	data[2] = val >> 8;
	data[1] = val >> 16;
	data[0] = val >> 24;
}

