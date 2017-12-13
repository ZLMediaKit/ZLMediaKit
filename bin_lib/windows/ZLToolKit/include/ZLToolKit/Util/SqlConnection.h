/*
 * SqlConnection.h
 *
 *  Created on: 2015年10月29日
 *      Author: root
 */

#ifndef SQL_SQLCONNECTION_H_
#define SQL_SQLCONNECTION_H_

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include "Util/logger.h"

#if defined(_WIN32)
#include <mysql.h>
#pragma  comment (lib,"libmysql") 
#else
#include <mysql/mysql.h>
#endif // defined(_WIN32)


using namespace std;

namespace ZL {
namespace Util {

class SqlConnection {
public:
	SqlConnection(const string &url, unsigned short port, const string &dbname,
			const string &user, const string &password,const string &character = "utf8mb4");
	virtual ~SqlConnection();

	template<typename ...Args>
	int64_t query(int64_t &rowId, const char *fmt, Args && ...arg) {
		check();
		string tmp = queryString(fmt, std::forward<Args>(arg)...);
		if (mysql_query(&sql, tmp.c_str())) {
			WarnL << mysql_error(&sql) << ":" << tmp << endl;
			return -1;
		}
		rowId=mysql_insert_id(&sql);
		return mysql_affected_rows(&sql);
	}

	int64_t query(int64_t &rowId,const char *str) {
		check();
		if (mysql_query(&sql, str)) {
			WarnL << mysql_error(&sql) << ":" << str << endl;
			return -1;
		}
		rowId=mysql_insert_id(&sql);
		return mysql_affected_rows(&sql);
	}
	template<typename ...Args>
	int64_t query(int64_t &rowId,vector<vector<string>> &ret, const char *fmt,
			Args && ...arg) {
		check();
		string tmp = queryString(fmt, std::forward<Args>(arg)...);
		if (mysql_query(&sql, tmp.c_str())) {
			WarnL << mysql_error(&sql)  << ":" << tmp << endl;
			return -1;
		}
		ret.clear();
		MYSQL_RES *res = mysql_store_result(&sql);
		if (!res) {
			rowId=mysql_insert_id(&sql);
			return mysql_affected_rows(&sql);
		}
		MYSQL_ROW row;
		unsigned int column = mysql_num_fields(res);
		while ((row = mysql_fetch_row(res)) != NULL) {
			ret.emplace_back();
			auto &back = ret.back();
			for (unsigned int i = 0; i < column; i++) {
				back.emplace_back(row[i] ? row[i] : "");
			}
		}
		mysql_free_result(res);
		rowId=mysql_insert_id(&sql);
		return mysql_affected_rows(&sql);
	}
	int64_t query(int64_t &rowId,vector<vector<string>> &ret, const char *str) {
		check();
		if (mysql_query(&sql, str)) {
			WarnL << mysql_error(&sql)  << ":" << str << endl;
			return -1;
		}
		ret.clear();
		MYSQL_RES *res = mysql_store_result(&sql);
		if (!res) {
			rowId=mysql_insert_id(&sql);
			return mysql_affected_rows(&sql);
		}
		MYSQL_ROW row;
		unsigned int column = mysql_num_fields(res);
		while ((row = mysql_fetch_row(res)) != NULL) {
			ret.emplace_back();
			auto &back = ret.back();
			for (unsigned int i = 0; i < column; i++) {
				back.emplace_back(row[i] ? row[i] : "" );
			}
		}
		mysql_free_result(res);
		rowId=mysql_insert_id(&sql);
		return mysql_affected_rows(&sql);
	}
	template<typename ...Args>
	static string queryString(const char *fmt, Args && ...arg) {
		char *ptr_out = NULL;
		asprintf(&ptr_out, fmt, arg...);
		string ret(ptr_out);
		if (ptr_out) {
			free(ptr_out);
		}
		return ret;
	}
	static string queryString(const char *fmt) {
		return fmt;
	}
	string &escape(string &str) {
		char *out = new char[str.length() * 2 + 1];
		mysql_real_escape_string(&sql, out, str.c_str(), str.size());
		str.assign(out);
		delete [] out;
		return str;
	}
private:
	MYSQL sql;
	inline void check() {
		if (mysql_ping(&sql) != 0) {
			throw runtime_error("MYSQL连接异常!");
		}
	}
};

} /* namespace util */
} /* namespace ZL */
#endif /* SQL_SQLCONNECTION_H_ */
