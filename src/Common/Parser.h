//
// Created by xzl on 2019/6/28.
//

#ifndef ZLMEDIAKIT_PARSER_H
#define ZLMEDIAKIT_PARSER_H

#include <map>
#include <string>
#include "Util/util.h"
using namespace std;
using namespace toolkit;

namespace mediakit{

string FindField(const char *buf, const char *start, const char *end, int bufSize = 0);

struct StrCaseCompare {
    bool operator()(const string &__x, const string &__y) const {
        return strcasecmp(__x.data(), __y.data()) < 0;
    }
};


class StrCaseMap : public multimap<string, string, StrCaseCompare>{
    public:
    typedef multimap<string, string, StrCaseCompare> Super ;
    StrCaseMap() = default;
    ~StrCaseMap() = default;

    string &operator[](const string &k){
        auto it = find(k);
        if(it == end()){
            it = Super::emplace(k,"");
        }
        return it->second;
    }

    template <typename V>
    void emplace(const string &k, V &&v) {
        auto it = find(k);
        if(it != end()){
            return;
        }
        Super::emplace(k,std::forward<V>(v));
    }

    template <typename V>
    void emplace_force(const string k , V &&v) {
        Super::emplace(k,std::forward<V>(v));
    }
};

class Parser {
    public:
    Parser() {}

    virtual ~Parser() {}

    void Parse(const char *buf) {
        //解析
        const char *start = buf;
        Clear();
        while (true) {
            auto line = FindField(start, NULL, "\r\n");
            if (line.size() == 0) {
                break;
            }
            if (start == buf) {
                _strMethod = FindField(line.data(), NULL, " ");
                _strFullUrl = FindField(line.data(), " ", " ");
                auto args_pos = _strFullUrl.find('?');
                if (args_pos != string::npos) {
                    _strUrl = _strFullUrl.substr(0, args_pos);
                    _params = _strFullUrl.substr(args_pos + 1);
                    _mapUrlArgs = parseArgs(_params);
                } else {
                    _strUrl = _strFullUrl;
                }
                _strTail = FindField(line.data(), (_strFullUrl + " ").data(), NULL);
            } else {
                auto field = FindField(line.data(), NULL, ": ");
                auto value = FindField(line.data(), ": ", NULL);
                if (field.size() != 0) {
                    _mapHeaders.emplace_force(field,value);
                }
            }
            start = start + line.size() + 2;
            if (strncmp(start, "\r\n", 2) == 0) { //协议解析完毕
                _strContent = FindField(start, "\r\n", NULL);
                break;
            }
        }
    }

    const string &Method() const {
        //rtsp方法
        return _strMethod;
    }

    const string &Url() const {
        //rtsp url
        return _strUrl;
    }

    const string &FullUrl() const {
        //rtsp url with args
        return _strFullUrl;
    }

    const string &Tail() const {
        //RTSP/1.0
        return _strTail;
    }

    const string &operator[](const char *name) const {
        //rtsp field
        auto it = _mapHeaders.find(name);
        if (it == _mapHeaders.end()) {
            return _strNull;
        }
        return it->second;
    }

    const string &Content() const {
        return _strContent;
    }

    void Clear() {
        _strMethod.clear();
        _strUrl.clear();
        _strFullUrl.clear();
        _params.clear();
        _strTail.clear();
        _strContent.clear();
        _mapHeaders.clear();
        _mapUrlArgs.clear();
    }
    const string &Params() const {
        return _params;
    }

    void setUrl(const string &url) {
        this->_strUrl = url;
    }

    void setContent(const string &content) {
        this->_strContent = content;
    }

    StrCaseMap &getValues() const {
        return _mapHeaders;
    }

    StrCaseMap &getUrlArgs() const {
        return _mapUrlArgs;
    }

    static StrCaseMap parseArgs(const string &str, const char *pair_delim = "&", const char *key_delim = "=") {
        StrCaseMap ret;
        auto arg_vec = split(str, pair_delim);
        for (string &key_val : arg_vec) {
            auto key = FindField(key_val.data(), NULL, key_delim);
            auto val = FindField(key_val.data(), key_delim, NULL);
            ret.emplace_force(trim(key),trim(val));
        }
        return ret;
    }

private:
    string _strMethod;
    string _strUrl;
    string _strTail;
    string _strContent;
    string _strNull;
    string _strFullUrl;
    string _params;
    mutable StrCaseMap _mapHeaders;
    mutable StrCaseMap _mapUrlArgs;
};


}//namespace mediakit

#endif //ZLMEDIAKIT_PARSER_H
