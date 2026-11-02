/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 * Use of this source code is governed by the LICENSE file in the root of the source tree.
 */

#include <memory>
#include <stdexcept>
#include <string>
#include "Util/logger.h"

#ifdef ENABLE_MP4
#include "Record/MP4Demuxer.h"

using namespace mediakit;

static void require(bool condition, const char *message) {
	if (!condition) {
		throw std::runtime_error(message);
	}
}

static size_t readToEnd(MultiMP4Demuxer &demuxer) {
	bool key = false;
	bool eof = false;
	size_t frames = 0;
	// Also bound the number of empty frames returned for unsupported tracks.
	for (size_t reads = 0; reads < 10000000 && !eof; ++reads) {
		if (demuxer.readFrame(key, eof)) {
			++frames;
		}
	}
	require(eof, "demuxer did not reach EOF");
	require(frames > 0, "no supported frames were read");
	key = true;
	eof = false;
	require(!demuxer.readFrame(key, eof) && eof && !key, "repeated EOF read failed");
	return frames;
}
#endif

static void showHelp(const char *program) {
	InfoL << "Usage: " << program << " <file.mp4>\n"
		  << "       " << program << " [-h | --help]\n"
		  << "Checks EOF, repeated seeks, duration and two-file boundaries.\n"
		  << "Example: " << program << " \"path/to/video.mp4\"";
}

int main(int argc, char **argv) {
	toolkit::Logger::Instance().add(std::make_shared<toolkit::ConsoleChannel>());
	if (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
		showHelp(argv[0]);
		return 0;
	}
	if (argc != 2) {
		ErrorL << "Provide exactly one MP4 file path.";
		showHelp(argv[0]);
		return 2;
	}
#ifdef ENABLE_MP4
	try {
		MultiMP4Demuxer demuxer;
		bool key = true;
		bool eof = false;
		require(!demuxer.readFrame(key, eof) && eof && !key, "empty demuxer read failed");
		demuxer.openMP4(argv[1]);
		const auto duration = demuxer.getDurationMS();
		require(duration > 0, "MP4 duration is zero");
		require(demuxer.seekTo(-1) < 0, "negative seek succeeded");
		require(demuxer.seekTo(static_cast<int64_t>(duration)) < 0, "end boundary seek succeeded");
		require(demuxer.seekTo(0) >= 0, "initial seek failed");
		const auto first_count = readToEnd(demuxer);
		for (int i = 0; i < 2; ++i) {
			require(demuxer.seekTo(0) >= 0, "repeat seek failed");
			require(readToEnd(demuxer) == first_count, "repeat frame count changed");
		}
		demuxer.closeMP4();
		require(demuxer.getDurationMS() == 0, "close did not clear duration");
		key = true;
		eof = false;
		require(!demuxer.readFrame(key, eof) && eof && !key, "read after close failed");

		demuxer.openMP4(std::string(argv[1]) + ";" + argv[1]);
		require(demuxer.getDurationMS() == duration * 2, "combined duration incorrect");
		require(demuxer.seekTo(0) >= 0, "combined initial seek failed");
		require(readToEnd(demuxer) == first_count * 2, "combined frame count incorrect");
		require(demuxer.seekTo(static_cast<int64_t>(duration)) >= 0, "segment boundary seek failed");
		require(readToEnd(demuxer) == first_count, "segment boundary frame count incorrect");
		InfoL << "MP4 demuxer repeat checks passed";
		return 0;
	} catch (const std::exception &ex) {
		ErrorL << ex.what();
		return 1;
	}
#else
	ErrorL << "MP4 support is disabled (ENABLE_MP4).";
	return 1;
#endif
}
