/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SEMAPHORE_H_
#define SEMAPHORE_H_

/*
 * 目前发现信号量在32位的系统上有问题，
 * 休眠的线程无法被正常唤醒，先禁用之
#if defined(__linux__)
#include <semaphore.h>
#define HAVE_SEM
#endif //HAVE_SEM
*/

#include <mutex>
#include <condition_variable>

namespace toolkit {

class semaphore {
public:
    explicit semaphore(size_t initial = 0) {
#if defined(HAVE_SEM)
        sem_init(&_sem, 0, initial);
#else
        _count = 0;
#endif
    }

    ~semaphore() {
#if defined(HAVE_SEM)
        sem_destroy(&_sem);
#endif
    }

    void post(size_t n = 1) {
#if defined(HAVE_SEM)
        while (n--) {
            sem_post(&_sem);
        }
#else
        std::unique_lock<std::recursive_mutex> lock(_mutex);
        _count += n;
        if (n == 1) {
            _condition.notify_one();
        } else {
            _condition.notify_all();
        }
#endif
    }

    void wait() {
#if defined(HAVE_SEM)
        sem_wait(&_sem);
#else
        std::unique_lock<std::recursive_mutex> lock(_mutex);
        while (_count == 0) {
            _condition.wait(lock);
        }
        --_count;
#endif
    }

private:
#if defined(HAVE_SEM)
    sem_t _sem;
#else
    size_t _count;
    std::recursive_mutex _mutex;
    std::condition_variable_any _condition;
#endif
};

} /* namespace toolkit */
#endif /* SEMAPHORE_H_ */
