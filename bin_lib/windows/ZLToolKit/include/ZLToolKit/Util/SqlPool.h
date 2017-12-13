/*
 * SqlPool.h
 *
 *  Created on: 2015年10月29日
 *      Author: root
 */

#ifndef SQL_SQLPOOL_H_
#define SQL_SQLPOOL_H_

#include <deque>
#include <mutex>
#include <memory>
#include <sstream>
#include <functional>
#include "logger.h"
#include "SqlConnection.h"
#include "Thread/ThreadPool.h"
#include "Util/ResourcePool.h"
#include "Thread/AsyncTaskThread.h"

using namespace std;
using namespace ZL::Thread;

namespace ZL {
namespace Util {
template<int poolSize = 10>
class _SqlPool {
public:
	typedef ResourcePool<SqlConnection, poolSize> PoolType;
	typedef vector<vector<string> > SqlRetType;
	static _SqlPool &Instance() {
		static _SqlPool *pool(new _SqlPool());
		return *pool;
	}
	static void Destory(){
		delete &Instance();
	}
	void reSize(int size) {
		if (size < 0) {
			return;
		}
		poolsize = size;
		pool->reSize(size);
		threadPool.reset(new ThreadPool(poolsize));
	}
	template<typename ...Args>
	void Init(Args && ...arg) {
		pool.reset(new PoolType(std::forward<Args>(arg)...));
		pool->obtain();
	}

	template<typename ...Args>
	int64_t query(const char *fmt, Args && ...arg) {
		string sql = SqlConnection::queryString(fmt, std::forward<Args>(arg)...);
		doQuery(sql);
		return 0;
	}
	int64_t query(const string &sql) {
		doQuery(sql);
		return 0;
	}

	template<typename ...Args>
	int64_t query(int64_t &rowID,vector<vector<string>> &ret, const char *fmt,
			Args && ...arg) {
		return _query(rowID,ret, fmt, std::forward<Args>(arg)...);
	}

	int64_t query(int64_t &rowID,vector<vector<string>> &ret, const string &sql) {
		return _query(rowID,ret, sql.c_str());
	}
	static const string &escape(const string &str) {
		try {
			//捕获创建对象异常
			_SqlPool::Instance().pool->obtain()->escape(
					const_cast<string &>(str));
		} catch (exception &e) {
			WarnL << e.what() << endl;
		}
		return str;
	}

private:
	_SqlPool() :
			threadPool(new ThreadPool(poolSize)), asyncTaskThread(10 * 1000) {
		poolsize = poolSize;
		asyncTaskThread.DoTaskDelay(reinterpret_cast<uint64_t>(this), 30 * 1000,
				[this]() {
					flushError();
					return true;
				});
	}
	inline void doQuery(const string &str,int tryCnt = 3) {
		auto lam = [this,str,tryCnt]() {
			int64_t rowID;
			auto cnt = tryCnt - 1;
			if(_query(rowID,str.c_str())==-2 && cnt > 0) {
				lock_guard<mutex> lk(error_query_mutex);
				sqlQuery query(str,cnt);
				error_query.push_back(query);
			}
		};
		threadPool->async(lam);
	}
	template<typename ...Args>
	inline int64_t _query(int64_t &rowID,Args &&...arg) {
		typename PoolType::ValuePtr mysql;
		try {
			//捕获执行异常
			mysql = pool->obtain();
			return mysql->query(rowID,std::forward<Args>(arg)...);
		} catch (exception &e) {
			pool->quit(mysql);
			WarnL << e.what() << endl;
			return -2;
		}
	}
	void flushError() {
		decltype(error_query) query_copy;
		error_query_mutex.lock();
		query_copy.swap(error_query);
		error_query_mutex.unlock();
		if (query_copy.size() == 0) {
			return;
		}
		for (auto &query : query_copy) {
			doQuery(query.sql_str,query.tryCnt);
		}
	}
	virtual ~_SqlPool() {
		asyncTaskThread.CancelTask(reinterpret_cast<uint64_t>(this));
		flushError();
		threadPool.reset();
		pool.reset();
		InfoL;
	}
	std::shared_ptr<ThreadPool> threadPool;
	mutex error_query_mutex;
	class sqlQuery
	{
	public:
		sqlQuery(const string &sql,int cnt):sql_str(sql),tryCnt(cnt){}
		string sql_str;
		int tryCnt = 0;
	} ;
	deque<sqlQuery> error_query;

	std::shared_ptr<PoolType> pool;
	AsyncTaskThread asyncTaskThread;
	unsigned int poolsize;
}
;
typedef _SqlPool<1> SqlPool;

class SqlStream {
public:
	SqlStream(const char *_sql) :
			sql(_sql) {
		startPos = 0;
	}
	~SqlStream() {

	}

	template<typename T>
	SqlStream& operator <<(const T& data) {
		auto pos = sql.find_first_of('?', startPos);
		if (pos == string::npos) {
			return *this;
		}
		str_tmp.str("");
		str_tmp << data;
		string str = str_tmp.str();
		startPos = pos + str.size();
		sql.replace(pos, 1, str);
		return *this;
	}
	const string& operator <<(std::ostream&(*f)(std::ostream&)) const {
		return sql;
	}
private:
	stringstream str_tmp;
	string sql;
	string::size_type startPos;
};

class SqlWriter {
public:
	SqlWriter(const char *_sql,bool _throwAble = true) :
			sqlstream(_sql),throwAble(_throwAble) {
	}
	~SqlWriter() {

	}
	template<typename T>
	SqlWriter& operator <<(const T& data) {
		sqlstream << data;
		return *this;
	}

	void operator <<(std::ostream&(*f)(std::ostream&)) {
		SqlPool::Instance().query(sqlstream << endl);
	}
	int64_t operator <<(vector<vector<string>> &ret) {
		affectedRows = SqlPool::Instance().query(rowId,ret, sqlstream << endl);
		if(affectedRows < 0 && throwAble){
			throw std::runtime_error("operate database failed");
		}
		return affectedRows;
	}
	int64_t getRowID() const {
		return rowId;
	}

	int64_t getAffectedRows() const {
		return affectedRows;
	}

private:
	SqlStream sqlstream;
	int64_t rowId = 0;
	int64_t affectedRows = -1;
	bool throwAble = true;
};

} /* namespace mysql */
} /* namespace im */

#endif /* SQL_SQLPOOL_H_ */
