/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <memory.h>
#include <set>
#include "Util/CMD.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/File.h"

using namespace std;
using namespace toolkit;

class CMD_main : public CMD {
public:
    CMD_main() {
        _parser.reset(new OptionParser(nullptr));
        (*_parser) << Option('f',/*该选项简称，如果是\x00则说明无简称*/
                             "filter",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             "c,cpp,cxx,c,h,hpp",/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "文件后缀过滤器",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('i',/*该选项简称，如果是\x00则说明无简称*/
                             "in",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             nullptr,/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "文件夹或文件",/*该选项说明文字*/
                             nullptr);
    }

    virtual ~CMD_main() {}
};

vector<string> split(const string& s, const char *delim) {
    vector<string> ret;
    int last = 0;
    int index = s.find(delim, last);
    while (index != string::npos) {
        if (index - last >= 0) {
            ret.push_back(s.substr(last, index - last));
        }
        last = index + strlen(delim);
        index = s.find(delim, last);
    }
    if (!s.size() || s.size() - last >= 0) {
        ret.push_back(s.substr(last));
    }
    return ret;
}

void process_file(const char *file) {
    auto str = File::loadFile(file);
    if (str.empty()) {
        return;
    }
    auto lines = ::split(str, "\n");
    deque<string> lines_copy;
    for (auto &line : lines) {
        if(line.empty()){
            lines_copy.push_back("");
            continue;
        }
        string line_copy;
        bool flag = false;
        int i = 0;
        for (auto &ch : line) {
            ++i;
            switch (ch) {
                case '\t' :
                    line_copy.append("    ");
                    break;
                case ' ':
                    line_copy.push_back(ch);
                    break;
                default:
                    line_copy.push_back(ch);
                    flag = true;
                    break;
            }
            if (flag) {
                line_copy.append(line.substr(i));
                break;
            }
        }
        lines_copy.push_back(line_copy);
    }
    str.clear();
    for (auto &line : lines_copy) {
        str.append(line);
        str.push_back('\n');
    }
    if(!lines_copy.empty()){
        str.pop_back();
    }
    File::saveFile(str, file);
}

/// 这个程序是为了统一替换tab为4个空格
int main(int argc, char *argv[]) {
    CMD_main cmd_main;
    try {
        cmd_main.operator()(argc, argv);
    } catch (std::exception &ex) {
        cout << ex.what() << endl;
        return -1;
    }

    string path = cmd_main["in"];
    string filter = cmd_main["filter"];
    auto vec = ::split(filter, ",");

    set<string> filter_set;
    for (auto ext : vec) {
        filter_set.emplace(ext);
    }

    bool no_filter = filter_set.find("*") != filter_set.end();
    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    File::scanDir(path, [&](const string &path, bool isDir) {
        if (isDir) {
            return true;
        }
        if (!no_filter) {
            //开启了过滤器
            auto pos = strstr(path.data(), ".");
            if (pos == nullptr) {
                //没有后缀
                return true;
            }
            auto ext = pos + 1;
            if (filter_set.find(ext) == filter_set.end()) {
                //后缀不匹配
                return true;
            }
        }
        //该文件匹配
        process_file(path.data());
        return true;
    }, true);
    return 0;
}
