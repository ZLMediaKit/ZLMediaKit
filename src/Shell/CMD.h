/*
 * CMD.h
 *
 *  Created on: 2016年9月26日
 *      Author: xzl
 */

#ifndef SRC_SHELL_CMD_H_
#define SRC_SHELL_CMD_H_

#if !defined(_WIN32)
#include <getopt.h>
#endif //!defined(_WIN32)

#include <string>
#include <vector>
#include <iostream>
#include <functional>
#include <unordered_map>
#include "Util/util.h"
#include "Util/logger.h"

using namespace std;
using namespace ZL::Util;

namespace ZL {
namespace Shell {

class OutStream {
public:
	virtual ~OutStream() {
	}
	virtual void response(const string &str) =0;
	virtual string &operator[](const string &) =0;
	virtual int erase(const string &) =0;
};

class Option {
public:
	typedef function<bool(OutStream *stream, const char *arg)> OptionHandler;
	enum ArgType {
		ArgNone = no_argument,
		ArgRequired = required_argument,
		ArgOptional = optional_argument
	};
	Option() {
	}
	Option(char _shortOpt, const char *_longOpt, enum ArgType _argType,
			const char *_des, const OptionHandler &_cb) {
		shortOpt = _shortOpt;
		longOpt = _longOpt;
		argType = _argType;
		des = _des;
		cb = _cb;
	}
	virtual ~Option() {
	}
	bool operator()(OutStream *stream, const char *arg){
		return cb ? cb(stream,arg): true;
	}
private:
	friend class OptionParser;
	char shortOpt;
	string longOpt;
	enum ArgType argType;
	string des;
	OptionHandler cb;
};

class OptionParser {
public:
	typedef function< void(OutStream *stream, const unordered_multimap<char, string> &)> OptionCompleted;
	OptionParser(const OptionCompleted &_cb) {
		onCompleted = _cb;
		helper = Option('h', "help", Option::ArgNone, "print this help", [this](OutStream *stream,const char *arg)->bool {
			_StrPrinter printer;
			for (auto &pr : options) {
				printer<<"\t-"<<pr.first<<"\t--"<<pr.second.longOpt<<"\t"<<pr.second.des<<"\r\n";
			}
			auto sendStr=printer<<endl;
			stream->response(sendStr);
			return false;
		});
	}
	virtual ~OptionParser() {
	}
	OptionParser &operator <<(const Option &option) {
		options.emplace(option.shortOpt, option);
		return *this;
	}
	void operator <<(ostream&(*f)(ostream&)) {
		str_shortOpt.clear();
		vec_longOpt.clear();
		(*this) << helper;
		struct option tmp;
		for (auto &pr : options) {
			//long opt
			tmp.name = pr.second.longOpt.data();
			tmp.has_arg = pr.second.argType;
			tmp.flag = NULL;
			tmp.val = pr.first;
			vec_longOpt.emplace_back(tmp);
			//short opt
			str_shortOpt.push_back(pr.first);
			switch (pr.second.argType) {
			case Option::ArgRequired:
				str_shortOpt.append(":");
				break;
			case Option::ArgOptional:
				str_shortOpt.append("::");
				break;
			default:
				break;
			}
		}
		tmp.flag=0;
		tmp.name=0;
		tmp.has_arg=0;
		tmp.val=0;
		vec_longOpt.emplace_back(tmp);

	}
	bool operator ()(OutStream *stream, int argc, char *argv[]) {
		lock_guard<mutex> lck(mtx_opt);
		unordered_multimap<char, string> allArg;
		int opt;
		optind = 0;
		while ((opt = getopt_long(argc, argv, &str_shortOpt[0], &vec_longOpt[0],NULL)) != -1) {
			auto it = options.find(opt);
			if (it == options.end()) {
				string sendStr = StrPrinter << "\t无法识别的选项,请输入\"-h\"获取帮助.\r\n" << endl;
				stream->response(sendStr);
				return true;
			}
			allArg.emplace(it->first, optarg ? optarg : "");
			if (!it->second(stream, optarg)) {
				return true;
			}
			optarg = NULL;
		}
		if(!allArg.size() && options.size()){
			helper(stream,"");
			return true;
		}
		if (onCompleted) {
			onCompleted(stream, allArg);
		}
		return true;
	}
private:
	unordered_map<char, Option> options;
	OptionCompleted onCompleted;
	vector<struct option> vec_longOpt;
	string str_shortOpt;
	Option helper;
	static mutex mtx_opt;
};

class CMD {
public:
	CMD();
	virtual ~CMD();
	virtual const char *description() const {
		return "description";
	}
	bool operator ()(OutStream *stream, int argc, char *argv[]) const {
		return (*parser)(stream, argc, argv);
	}
protected:
	mutable std::shared_ptr<OptionParser> parser;
};
template<typename C>
class CMDInstance {
public:
	static CMD &Instance() {
		static C instance;
		return (CMD &) instance;
	}
};
class CMD_help: public CMD {
public:
	CMD_help();
	virtual ~CMD_help() {
	}
	const char *description() const override {
		return "打印帮助信息.";
	}
};

class CMD_rtsp: public CMD {
public:
	CMD_rtsp();
	virtual ~CMD_rtsp() {
	}
	const char *description() const override {
		return "查看rtsp服务器相关信息.";
	}
};
class CMD_rtmp: public CMD {
public:
	CMD_rtmp();
	virtual ~CMD_rtmp() {
	}
	const char *description() const override {
		return "查看rtmp服务器相关信息.";
	}
};








































} /* namespace Shell */
} /* namespace ZL */

#endif /* SRC_SHELL_CMD_H_ */
