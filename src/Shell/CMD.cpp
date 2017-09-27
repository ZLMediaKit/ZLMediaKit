/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
	(*parser) << Option('c', "cmd", Option::ArgNone, "list all command", [](OutStream *stream,const char *arg) {
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
	(*parser) << Option('l', "list", Option::ArgNone, "list all media source of rtsp", [](OutStream *stream,const char *arg) {
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
	(*parser) << Option('l', "list", Option::ArgNone, "list all media source of rtmp", [](OutStream *stream,const char *arg) {
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


