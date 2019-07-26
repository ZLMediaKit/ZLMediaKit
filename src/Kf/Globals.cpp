//
// Created by 陈磊 on 2019-06-24.
//
#include <string>
#include "Player/PlayerProxy.h"
#include "Globals.h"
#include <jsoncpp/value.h>
#include <jsoncpp/json.h>
#include "Util/File.h"
#include <vector>
#include <dirent.h>
#include <regex>

string getProxyKey(const string &vhost, const string &app, const string &stream) {
    return vhost + "/" + app + "/" + stream;
}


using file_filter_type=std::function<bool(const char *, const char *, const char *)>;

/*
 * 列出指定目录的所有文件(不包含目录)执行，对每个文件执行filter过滤器，
 * filter返回true时将文件名全路径加入std::vector
 * sub为true时为目录递归
 * 返回每个文件的全路径名
*/
vector<string> forEachFile(const std::string &dir_name, file_filter_type filter, bool sub = false) {
    std::vector<string> v;
    auto dir = opendir(dir_name.data());
    struct dirent *ent;
    if (dir) {
        while ((ent = readdir(dir)) != NULL) {
            auto p = std::string(dir_name).append({'/'}).append(ent->d_name);
            if (sub) {
                if (0 == strcmp(ent->d_name, "..") || 0 == strcmp(ent->d_name, ".")) {
                    continue;
                } else if (File::is_dir(p.c_str())) {
                    auto r = forEachFile(p, filter, sub);
                    v.insert(v.end(), r.begin(), r.end());
                    continue;
                }
            }
            if (sub || !File::is_dir(p.c_str())) {
                //如果是文件，则调用过滤器filter
                if (filter(dir_name.data(), ent->d_name, p.c_str())) {
                    v.emplace_back(p);
                }
            }
        }
        closedir(dir);
    }
    return v;
}

/**
 * 清除无效的录像文件
 *  如:  .14-33-01.mp4 这种前面有个.的文件名格式的录像会被清除
 * @param recordFilePath
 */
void clearInvalidRecord(const string &recordFilePath) {
    forEachFile(recordFilePath,
            // filter函数，lambda表达式
                [&](const char *path, const char *name, const char *fullpath) {
                    //判断时临时录像文件
                    if (std::regex_match(name, std::regex("\\.\\d{2}-\\d{2}-\\d{2}\\.mp4"))) {
                        File::delete_file(fullpath);
                        InfoL << "清理无效的临时录像文件成功:" << fullpath;
                    }
                    //因为文件已经已经在lambda表达式中处理了，
                    //不需要for_each_file返回文件列表，所以这里返回false
                    return false;
                }, true//递归子目录
    );
}


int getNumberOfDays(int year, int month) {
    //leap year condition, if month is 2
    if (month == 2) {
        if ((year % 400 == 0) || (year % 4 == 0 && year % 100 != 0))
            return 29;
        else
            return 28;
    }
        //months which has 31 days
    else if (month == 1 || month == 3 || month == 5 || month == 7 || month == 8
             || month == 10 || month == 12)
        return 31;
    else
        return 30;
}

int getNumberOfDays(const std::string &monthStr) {
    string year = monthStr.substr(0, 4);
    string month = monthStr.substr(4);
    return getNumberOfDays(atoi(year.c_str()), atoi(month.c_str()));
}


/****
 * chenxiaolei ZlToolKit Util/util.cpp 中的 split 有些问题, 会忽略空值,不能得到预期的值, 固增加一个split2
 *   错误:  ZlToolKit::split(",,,,,", ",").size()    == 0
 *   正确:            split2(",,,,,",",").size()    == 5
 */
std::vector<std::string> split2(std::string stringToBeSplitted, std::string delimeter) {
    std::vector<std::string> splittedString;
    int startIndex = 0;
    int  endIndex = 0;
    while( (endIndex = stringToBeSplitted.find(delimeter, startIndex)) < stringToBeSplitted.size() )
    {
        std::string val = stringToBeSplitted.substr(startIndex, endIndex - startIndex);
        splittedString.push_back(val);
        startIndex = endIndex + delimeter.size();
    }
    if(startIndex < stringToBeSplitted.size())
    {
        std::string val = stringToBeSplitted.substr(startIndex);
        splittedString.push_back(val);
    }
    return splittedString;
}