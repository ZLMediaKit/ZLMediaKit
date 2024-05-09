/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SQL_SQLCONNECTION_H_
#define SQL_SQLCONNECTION_H_

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>
#include <list>
#include <deque>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include "logger.h"
#include "util.h"
#include <mysql.h>

#if defined(_WIN32)
#pragma  comment (lib,"libmysql") 
#endif

namespace toolkit {

/**
 * 数据库异常类
 */
class SqlException : public std::exception {
public:
    SqlException(const std::string &sql, const std::string &err) {
        _sql = sql;
        _err = err;
    }

    virtual const char *what() const noexcept {
        return _err.data();
    }

    const std::string &getSql() const {
        return _sql;
    }

private:
    std::string _sql;
    std::string _err;
};

/**
 * mysql连接
 */
class SqlConnection {
public:
    /**
     * 构造函数
     * @param url 数据库地址
     * @param port 数据库端口号
     * @param dbname 数据库名
     * @param username 用户名
     * @param password 用户密码
     * @param character 字符集
     */
    SqlConnection(const std::string &url, unsigned short port,
                  const std::string &dbname, const std::string &username,
                  const std::string &password, const std::string &character = "utf8mb4") {
        mysql_init(&_sql);
        unsigned int timeout = 3;
        mysql_options(&_sql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
        if (!mysql_real_connect(&_sql, url.data(), username.data(),
                                password.data(), dbname.data(), port, nullptr, 0)) {
            mysql_close(&_sql);
            throw SqlException("mysql_real_connect", mysql_error(&_sql));
        }
        //兼容bool与my_bool
        uint32_t reconnect = 0x01010101;
        mysql_options(&_sql, MYSQL_OPT_RECONNECT, &reconnect);
        mysql_set_character_set(&_sql, character.data());
    }

    ~SqlConnection() {
        mysql_close(&_sql);
    }

    /**
     * 以printf样式执行sql,无数据返回
     * @param rowId insert时的插入rowid
     * @param fmt printf类型fmt
     * @param arg 可变参数列表
     * @return 影响行数
     */
    template<typename Fmt, typename ...Args>
    int64_t query(int64_t &rowId, Fmt &&fmt, Args &&...arg) {
        check();
        auto tmp = queryString(std::forward<Fmt>(fmt), std::forward<Args>(arg)...);
        if (doQuery(tmp)) {
            throw SqlException(tmp, mysql_error(&_sql));
        }
        rowId = mysql_insert_id(&_sql);
        return mysql_affected_rows(&_sql);
    }

    /**
     * 以printf样式执行sql,并且返回list类型的结果(不包含数据列名)
     * @param rowId insert时的插入rowid
     * @param ret 返回数据列表
     * @param fmt printf类型fmt
     * @param arg 可变参数列表
     * @return 影响行数
     */
    template<typename Fmt, typename ...Args>
    int64_t query(int64_t &rowId, std::vector<std::vector<std::string> > &ret, Fmt &&fmt, Args &&...arg) {
        return queryList(rowId, ret, std::forward<Fmt>(fmt), std::forward<Args>(arg)...);
    }

    template<typename Fmt, typename... Args>
    int64_t query(int64_t &rowId, std::vector<std::list<std::string>> &ret, Fmt &&fmt, Args &&...arg) {
        return queryList(rowId, ret, std::forward<Fmt>(fmt), std::forward<Args>(arg)...);
    }

    template<typename Fmt, typename ...Args>
    int64_t query(int64_t &rowId, std::vector<std::deque<std::string> > &ret, Fmt &&fmt, Args &&...arg) {
        return queryList(rowId, ret, std::forward<Fmt>(fmt), std::forward<Args>(arg)...);
    }

    /**
     * 以printf样式执行sql,并且返回Map类型的结果(包含数据列名)
     * @param rowId insert时的插入rowid
     * @param ret 返回数据列表
     * @param fmt printf类型fmt
     * @param arg 可变参数列表
     * @return 影响行数
     */
    template<typename Map, typename Fmt, typename ...Args>
    int64_t query(int64_t &rowId, std::vector<Map> &ret, Fmt &&fmt, Args &&...arg) {
        check();
        auto tmp = queryString(std::forward<Fmt>(fmt), std::forward<Args>(arg)...);
        if (doQuery(tmp)) {
            throw SqlException(tmp, mysql_error(&_sql));
        }
        ret.clear();
        MYSQL_RES *res = mysql_store_result(&_sql);
        if (!res) {
            rowId = mysql_insert_id(&_sql);
            return mysql_affected_rows(&_sql);
        }
        MYSQL_ROW row;
        unsigned int column = mysql_num_fields(res);
        MYSQL_FIELD *fields = mysql_fetch_fields(res);
        while ((row = mysql_fetch_row(res)) != nullptr) {
            ret.emplace_back();
            auto &back = ret.back();
            for (unsigned int i = 0; i < column; i++) {
                back[std::string(fields[i].name, fields[i].name_length)] = (row[i] ? row[i] : "");
            }
        }
        mysql_free_result(res);
        rowId = mysql_insert_id(&_sql);
        return mysql_affected_rows(&_sql);
    }

    std::string escape(const std::string &str) {
        char *out = new char[str.length() * 2 + 1];
        mysql_real_escape_string(&_sql, out, str.c_str(), str.size());
        std::string ret(out);
        delete[] out;
        return ret;
    }

    template<typename ...Args>
    static std::string queryString(const char *fmt, Args &&...arg) {
        char *ptr_out = nullptr;
        if (asprintf(&ptr_out, fmt, arg...) > 0 && ptr_out) {
            std::string ret(ptr_out);
            free(ptr_out);
            return ret;
        }
        return "";
    }

    template<typename ...Args>
    static std::string queryString(const std::string &fmt, Args &&...args) {
        return queryString(fmt.data(), std::forward<Args>(args)...);
    }

    static const char *queryString(const char *fmt) {
        return fmt;
    }

    static const std::string &queryString(const std::string &fmt) {
        return fmt;
    }

private:
    template<typename List, typename Fmt, typename... Args>
    int64_t queryList(int64_t &rowId, std::vector<List> &ret, Fmt &&fmt, Args &&...arg) {
        check();
        auto tmp = queryString(std::forward<Fmt>(fmt), std::forward<Args>(arg)...);
        if (doQuery(tmp)) {
            throw SqlException(tmp, mysql_error(&_sql));
        }
        ret.clear();
        MYSQL_RES *res = mysql_store_result(&_sql);
        if (!res) {
            rowId = mysql_insert_id(&_sql);
            return mysql_affected_rows(&_sql);
        }
        MYSQL_ROW row;
        unsigned int column = mysql_num_fields(res);
        while ((row = mysql_fetch_row(res)) != nullptr) {
            ret.emplace_back();
            auto &back = ret.back();
            for (unsigned int i = 0; i < column; i++) {
                back.emplace_back(row[i] ? row[i] : "");
            }
        }
        mysql_free_result(res);
        rowId = mysql_insert_id(&_sql);
        return mysql_affected_rows(&_sql);
    }

    inline void check() {
        if (mysql_ping(&_sql) != 0) {
            throw SqlException("mysql_ping", "Mysql connection ping failed");
        }
    }

    int doQuery(const std::string &sql) {
        return mysql_query(&_sql, sql.data());
    }

    int doQuery(const char *sql) {
        return mysql_query(&_sql, sql);
    }

private:
    MYSQL _sql;
};

} /* namespace toolkit */
#endif /* SQL_SQLCONNECTION_H_ */
