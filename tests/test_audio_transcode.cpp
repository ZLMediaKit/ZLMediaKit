#include "Codec/Transcode.h"
#include "Record/MP4Demuxer.h"
#include "Record/MP4Muxer.h"
#include "Extension/AAC.h"
#include "Extension/Opus.h"
#include "Util/logger.h"
using namespace mediakit;
struct TransCtx {
    using Ptr = std::shared_ptr<TransCtx>;
    TransCtx(const char *prefix, CodecId codec) {
        char path[256];
        Track::Ptr track;
        switch (codec) {
        case CodecAAC:
            track.reset(new AACTrack(44100, 1));
            sprintf(path, "%s_aac.mp4", prefix);
            break;
        case CodecOpus:
            track.reset(new OpusTrack());
            sprintf(path, "%s_opus.mp4", prefix);
            break;
        case CodecG711A:
        case CodecG711U:
            track.reset(new G711Track(codec, 8000, 1, 16));
            sprintf(path, "%s_711%c.mp4", prefix, codec == CodecG711A ? 'A' : 'U');
            break;
        default:
            return;
            break;
        }
        file.reset(new MP4Muxer());
        file->openMP4(path);
        file->addTrack(track);
        enc.reset(new FFmpegEncoder(track));
        enc->setOnEncode([this](const Frame::Ptr &frame) { file->inputFrame(frame); });
    }
    ~TransCtx() {
        enc = nullptr;
        file = nullptr;
    }
    void inputFrame(const FFmpegFrame::Ptr &frame) { enc->inputFrame(frame, false); }
    FFmpegEncoder::Ptr enc;
    std::shared_ptr<MP4Muxer> file;
};

int TranscodeAudio(const char *srcPath, const char *dstPath) {
    MP4Demuxer srcMp4;
    srcMp4.openMP4(srcPath);

    auto srcTrack = srcMp4.getTrack(TrackAudio);
    if (!srcTrack) {
        printf("unable to find audioTrack %s\n", srcPath);
        return -1;
    }
    std::vector<TransCtx::Ptr> trans;
    FFmpegDecoder audioDec(srcTrack);
    auto dstCodec = getCodecId(dstPath);
    if (!strcasecmp(dstPath, "aac"))
        dstCodec = CodecAAC;
    if (dstCodec != CodecInvalid) {
        std::string dstFile(srcPath);
        auto pos = dstFile.rfind('_');
        if (pos == dstFile.npos)
            pos = dstFile.rfind('.');
        if (pos != dstFile.npos)
            dstFile = dstFile.substr(0, pos);
        if (dstCodec == srcTrack->getCodecId()) {
            printf("same codec %s, skip transcode\n", dstPath);
            return 0;
        }
        trans.push_back(std::make_shared<TransCtx>(dstFile.c_str(), dstCodec));
    }
    else {
        for (auto codec : { CodecAAC, CodecOpus, CodecG711A, CodecG711U }) {
            if (codec == srcTrack->getCodecId())
                continue;
            trans.push_back(std::make_shared<TransCtx>(dstPath, codec));
        }
    }
    // srcTrack -> audioDec
    srcTrack->addDelegate(std::make_shared<FrameWriterInterfaceHelper>([&](const Frame::Ptr &frame) -> bool {
        audioDec.inputFrame(frame, true, false, false);
        return true;
    }));
    // audioDec -> audioEnc
    audioDec.setOnDecode([&](const FFmpegFrame::Ptr &frame) {
        for (TransCtx::Ptr p : trans)
            p->inputFrame(frame);
    });
    toolkit::Ticker tick;
    printf("startReadMp4 %" PRIu64 "ms\n", srcMp4.getDurationMS());
    bool key, eof;
    Frame::Ptr frame;
    while (true) {
        // srcMp4->srcTrack
        frame = srcMp4.readFrame(key, eof);
        if (eof) {
            printf("eof break loop, it tooks %" PRIu64 " ms\n", tick.elapsedTime());
            break;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("usage src.mp4 dst_prefix/codecName\n");
        return 0;
    }
    toolkit::Logger::Instance().add(std::make_shared<toolkit::ConsoleChannel>());
    try {
        return TranscodeAudio(argv[1], argv[2]);
    } catch (std::exception e) {
        printf("exception: %s\n", e.what());
        return -1;
    }
}