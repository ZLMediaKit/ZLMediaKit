/*
* Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
*
* This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
*
* Use of this source code is governed by MIT license that can be found in the
* LICENSE file in the root of the source tree. All contributing project authors
* may be found in the AUTHORS file in the root of the source tree.
*/

#ifndef SPEED_STATISTIC_H_
#define SPEED_STATISTIC_H_

#include "TimeTicker.h"

namespace toolkit {

class BytesSpeed {
public:
    BytesSpeed() = default;
    ~BytesSpeed() = default;

    /**
     * 添加统计字节
     */
    BytesSpeed &operator+=(size_t bytes) {
        _bytes += bytes;
        if (_bytes > 1024 * 1024) {
            //数据大于1MB就计算一次网速
            computeSpeed();
        }
        return *this;
    }

    /**
     * 获取速度，单位bytes/s
     */
    int getSpeed() {
        if (_ticker.elapsedTime() < 1000) {
            //获取频率小于1秒，那么返回上次计算结果
            return _speed;
        }
        return computeSpeed();
    }

private:
    int computeSpeed() {
        auto elapsed = _ticker.elapsedTime();
        if (!elapsed) {
            return _speed;
        }
        _speed = (int)(_bytes * 1000 / elapsed);
        _ticker.resetTime();
        _bytes = 0;
        return _speed;
    }

private:
    int _speed = 0;
    size_t _bytes = 0;
    Ticker _ticker;
};

} /* namespace toolkit */
#endif /* SPEED_STATISTIC_H_ */
