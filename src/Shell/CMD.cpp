/*
 * CMD.cpp
 *
 *  Created on: 2016年9月26日
 *      Author: xzl
 */

#include "CMD.h"
#include "ShellSession.h"
#include "Rtsp/RtspMediaSource.h"
#include "Rtmp/RtmpMediaSource.h"
using namespace ZL::Rtsp;
using namespace ZL::Rtmp;


namespace ZL {
namespace Shell {

mutex OptionParser::mtx_opt;

CMD::CMD() {
}

CMD::~CMD() {
}

CMD_help::CMD_help() {
	parser.reset( new OptionParser(nullptr));
	(*parser) << Option('c', "cmd", Option::ArgNone, "列出所有命令", [](OutStream *stream,const char *arg) {
		_StrPrinter printer;
		for (auto &pr : ShellSession::g_mapCmd) {
			printer << "\t" << pr.first << ":" << pr.second.description() << "\r\n";
		}
		stream->response(printer << endl);
		return false;
	}) << endl;
}

CMD_rtsp::CMD_rtsp() {
	parser.reset(new OptionParser(nullptr));
	(*parser) << Option('l', "list", Option::ArgNone, "列出所有媒体列表", [](OutStream *stream,const char *arg) {
		_StrPrinter printer;
		auto mediaSet = RtspMediaSource::getMediaSet();
		for (auto &src : mediaSet) {
			printer << "\t" << src << "\r\n";
		}
		stream->response(printer << endl);
		return false;
	}) << endl;
}

CMD_rtmp::CMD_rtmp() {
	parser.reset(new OptionParser(nullptr));
	(*parser) << Option('l', "list", Option::ArgNone, "列出所有媒体列表", [](OutStream *stream,const char *arg) {
		_StrPrinter printer;
		auto mediaSet = RtmpMediaSource::getMediaSet();
		for (auto &src : mediaSet) {
			printer << "\t" << src << "\r\n";
		}
		stream->response(printer << endl);
		return false;
	}) << endl;
}





















} /* namespace Shell */
} /* namespace ZL */


