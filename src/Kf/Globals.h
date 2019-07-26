//
// Created by 陈磊 on 2019-06-24.
//
#ifndef KF_GLOBALS_H
#define KF_GLOBALS_H

#include <string>
#include "Player/PlayerProxy.h"
#include <jsoncpp/value.h>
#include <jsoncpp/json.h>
#include <vector>


extern string getProxyKey(const string &vhost,const string &app,const string &stream);

//遍历文件夹
extern vector<string> forEachFile(const string &dir_name, function<bool(const char *, const char *,const char*)> filter, bool sub);

//清除无效的录像文件
extern void clearInvalidRecord(const string &recordFilePath);

//获取指定月份的天数
extern int getNumberOfDays(int year, int month);

//获取指定月份的天数(YYYYMM)
extern int getNumberOfDays(const std::string &monthStr);

std::vector<std::string>  split2(std::string stringToBeSplitted, std::string delimeter);

#endif //KF_GLOBALS_H