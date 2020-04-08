/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef __utils_h
#define __utils_h

#include <stdio.h>
#include <stdint.h>


uint32_t load_be32(const void *p);
uint16_t load_be16(const void *p);
uint32_t load_be24(const void *p);
uint32_t load_le32(const void *p);
void set_be24(void *p, uint32_t val);
void set_le32(void *p, uint32_t val);
void set_be32(void *p, uint32_t val);


#endif
