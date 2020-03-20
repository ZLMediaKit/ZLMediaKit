#include <stdlib.h>
#include <memory.h>

#if !defined(_WIN32)

#include <dirent.h>

#endif //!defined(_WIN32)

#include <set>
#include "Util/CMD.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/File.h"
#include "Util/uv_errno.h"

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

void process_file(const char *file) {
    auto str = File::loadFile(file);
    if (str.empty()) {
        return;
    }
    auto lines = split(str, "\n");
    deque<string> lines_copy;
    for (auto &line : lines) {
        string line_copy;
        bool flag = false;
        for (auto &ch : line) {
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
    File::saveFile(str, file);
}

int main(int argc, char *argv[]) {
    CMD_main cmd_main;
    try {
        cmd_main.operator()(argc, argv);
    } catch (std::exception &ex) {
        cout << ex.what() << endl;
        return -1;
    }

    bool rm_bom = cmd_main.hasKey("rm");
    string path = cmd_main["in"];
    string filter = cmd_main["filter"];
    auto vec = split(filter, ",");

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
