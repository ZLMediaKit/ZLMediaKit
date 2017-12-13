/*
 * RingBuffer.h
 *
 *  Created on: 2016年8月10日
 *      Author: xzl
 */

#ifndef UTIL_RINGBUFFER_H_
#define UTIL_RINGBUFFER_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <condition_variable>

using namespace std;

namespace ZL {
namespace Util {

//实现了一个一写多读得环形缓冲的模板类
template<typename T> class RingBuffer: public enable_shared_from_this<
		RingBuffer<T> > {
public:
	typedef std::shared_ptr<RingBuffer> Ptr;

	class RingReader {
	public:
		friend class RingBuffer;
		typedef std::shared_ptr<RingReader> Ptr;
		RingReader(const std::shared_ptr<RingBuffer> &_buffer,bool _useBuffer) {
			buffer = _buffer;
			curpos = _buffer->ringKeyPos;
			useBuffer = _useBuffer;
		}
		virtual ~RingReader() {
			auto strongBuffer = buffer.lock();
			if (strongBuffer) {
				strongBuffer->release(this);
			}
		}
		//重新定位读取器至最新的数据位置
		void reset(bool keypos = true) {
			auto strongBuffer = buffer.lock();
			if (strongBuffer) {
				if(keypos){
					curpos = strongBuffer->ringKeyPos; //定位读取器
				}else{
					curpos = strongBuffer->ringPos; //定位读取器
				}
			}
		}
		void setReadCB(const function<void(const T &)> &cb) {
			lock_guard<recursive_mutex> lck(mtxCB);
			readCB = cb;
			reset();
		}
		void setDetachCB(const function<void()> &cb) {
			lock_guard<recursive_mutex> lck(mtxCB);
			if (!cb) {
				detachCB = []() {};
			} else {
				detachCB = cb;
			}
		}
        
        const T* read(){
            auto strongBuffer=buffer.lock();
            if(!strongBuffer){
                return nullptr;
            }
            return read(strongBuffer.get());
        }
        
	private:
		void onRead(const T &data) {
			lock_guard<recursive_mutex> lck(mtxCB);
			if(!readCB){
				return;
			}

			if(!useBuffer){
				readCB(data);
			}else{
				const T *pkt  = nullptr;
				while((pkt = read())){
					readCB(*pkt);
				}
			}
		}
		void onDetach() const {
			lock_guard<recursive_mutex> lck(mtxCB);
			detachCB();
		}
		//读环形缓冲
		const T *read(RingBuffer *ring) {
			if (curpos == ring->ringPos) {
				return nullptr;
			}
			const T *data = &(ring->dataRing[curpos]); //返回包
			curpos = ring->next(curpos); //更新位置
			return data;
		}
        
		function<void(const T &)> readCB ;
		function<void(void)> detachCB = []() {};
		weak_ptr<RingBuffer> buffer;
		mutable recursive_mutex mtxCB;
		int curpos;
		bool useBuffer;
	};

	friend class RingReader;
	RingBuffer(int size = 0) {
        if(size <= 0){
            size = 32;
            canReSize = true;
        }
		ringSize = size;
		dataRing = new T[ringSize];
		ringPos = 0;
		ringKeyPos = 0;
	}
	virtual ~RingBuffer() {
		decltype(readerMap) mapCopy;
		{
			lock_guard<recursive_mutex> lck(mtx_reader);
			mapCopy.swap(readerMap);
		}
		for (auto &pr : mapCopy) {
			auto reader = pr.second.lock();
			if(reader){
				reader->onDetach();
			}
		}
		delete[] dataRing;
	}
#if defined(ENABLE_RING_USEBUF)
	std::shared_ptr<RingReader> attach(bool useBuffer = true) {
#else //ENABLE_RING_USEBUF
	std::shared_ptr<RingReader> attach(bool useBuffer = false) {
#endif //ENABLE_RING_USEBUF

		std::shared_ptr<RingReader> ptr(new RingReader(this->shared_from_this(),useBuffer));
		std::weak_ptr<RingReader> weakPtr = ptr;
		lock_guard<recursive_mutex> lck(mtx_reader);
		readerMap.emplace(ptr.get(),weakPtr);
		return ptr;
	}
	//写环形缓冲，非线程安全的
	void write(const T &in,bool isKey = true) {
		computeBestSize(isKey);
        dataRing[ringPos] = in;
        if (isKey) {
            ringKeyPos = ringPos; //设置读取器可以定位的点
        }
        ringPos = next(ringPos);
        decltype(readerMap) mapCopy;
        {
        	lock_guard<recursive_mutex> lck(mtx_reader);
        	mapCopy = readerMap;
        }
		for (auto &pr : mapCopy) {
			auto reader = pr.second.lock();
			if(reader){
				reader->onRead(in);
			}
		}
	}
	int readerCount(){
		lock_guard<recursive_mutex> lck(mtx_reader);
		return readerMap.size();
	}
private:
	T *dataRing;
	int ringPos;
	int ringKeyPos;
	int ringSize;
	//计算最佳环形缓存大小的参数
	int besetSize = 0;
	int totalCnt = 0;
	int lastKeyCnt = 0;
    bool canReSize = false;
            
	recursive_mutex mtx_reader;
	unordered_map<void *,std::weak_ptr<RingReader> > readerMap;

	inline int next(int pos) {
		//读取器下一位置
		if (pos > ringSize - 2) {
			return 0;
		} else {
			return pos + 1;
		}
	}

	void release(RingReader *reader) {
		lock_guard<recursive_mutex> lck(mtx_reader);
		readerMap.erase(reader);
	}
	void computeBestSize(bool isKey){
        if(!canReSize || besetSize){
			return;
		}
		totalCnt++;
		if(!isKey){
			return;
		}
		//关键帧
		if(lastKeyCnt){
			//计算两个I帧之间的包个数
			besetSize = totalCnt - lastKeyCnt;
			reSize();
			return;
		}
		lastKeyCnt = totalCnt;
	}
	void reSize(){
		ringSize = besetSize;
		delete [] dataRing;
		dataRing = new T[ringSize];
		ringPos = 0;
		ringKeyPos = 0;

		decltype(readerMap) mapCopy;
		{
			lock_guard<recursive_mutex> lck(mtx_reader);
			mapCopy = readerMap;
		}
		for (auto &pr : mapCopy) {
			auto reader = pr.second.lock();
			if(reader){
				reader->reset(true);
			}
		}

	}
};

} /* namespace Util */
} /* namespace ZL */

#endif /* UTIL_RINGBUFFER_H_ */
