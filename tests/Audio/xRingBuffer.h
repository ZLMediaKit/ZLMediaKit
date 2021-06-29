/* MIT License
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

#ifndef AUDIO_XRINGBUFFER_H
#define AUDIO_XRINGBUFFER_H

#include <vector>
#include <atomic>

template <typename T>
class xRingBuffer
{
public:
    xRingBuffer(int capacity=60)
            : capacity_(capacity)
            , num_datas_(0)
            , buffer_(capacity)
    { }

    virtual ~xRingBuffer() {	}

    bool Push(const T& data)
    {
        return pushData(std::forward<T>(data));
    }

    bool Push(T&& data)
    {
        return PushData(data);
    }

    bool Pop(T& data)
    {
        if(num_datas_ > 0) {
            data = std::move(buffer_[get_pos_]);
            Add(get_pos_);
            num_datas_--;
            return true;
        }

        return false;
    }

    bool IsFull()  const
    {
        return num_datas_==capacity_;
    }

    bool IsEmpty() const
    {
        return num_datas_==0;
    }

    int  Size() const
    {
        return num_datas_;
    }

private:
    template <typename F>
    bool PushData(F&& data)
    {
        if (num_datas_ < capacity_)
        {
            buffer_[put_pos_] = std::forward<F>(data);
            Add(put_pos_);
            num_datas_++;
            return true;
        }

        return false;
    }

    void Add(int& pos)
    {
        pos = (((pos+1)==capacity_) ? 0 : (pos+1));
    }

    int capacity_ = 0;
    int put_pos_ = 0;
    int get_pos_ = 0;

    std::atomic_int num_datas_;
    std::vector<T> buffer_;
};



#endif //AUDIO_XRINGBUFFER_H
