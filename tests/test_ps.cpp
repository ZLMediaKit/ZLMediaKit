#include "Common/config.h"
#include "Http/HttpSession.h"
#include "Network/TcpServer.h"
#include "Rtmp/RtmpSession.h"
#include "Rtp/Decoder.h"
#include "Rtp/RtpProcess.h"
#include "Rtsp/RtspSession.h"
#include "Util/File.h"
#include "Util/MD5.h"
#include "Util/SSLBox.h"
#include "Util/logger.h"
#include "Util/util.h"
#include <iostream>
#include <map>


using namespace std;
using namespace toolkit;
using namespace mediakit;

static semaphore sem;
class PsProcess : public MediaSinkInterface, public std::enable_shared_from_this<PsProcess> {
public:
    using Ptr = std::shared_ptr<PsProcess>;
    PsProcess() {
        MediaTuple media_info;
        media_info.vhost = DEFAULT_VHOST;
        media_info.app = "rtp";
        media_info.stream = "000001";

        _muxer = std::make_shared<MultiMediaSourceMuxer>(media_info, 0.0f, ProtocolOption());
    }
    ~PsProcess() {
    }

    bool inputFrame(const Frame::Ptr &frame) override {
        if (_muxer) {
            _muxer->inputFrame(frame);
            int64_t diff = frame->dts() - timeStamp_last;
            if (diff > 0 && diff < 500) {
                usleep(diff * 1000);
            } else {
                usleep(1 * 1000);
            }
            timeStamp_last = frame->dts();
        }
        return true;
    }
    bool addTrack(const Track::Ptr &track) override {
        if (_muxer) {
            return _muxer->addTrack(track);
        }
        return true;
    }
    void addTrackCompleted() override {
        if (_muxer) {
            _muxer->addTrackCompleted();
        }
    }
    void resetTracks() override {

    }
    virtual void flush() override {}

private:
    MultiMediaSourceMuxer::Ptr _muxer;
    uint64_t timeStamp = 0;
    uint64_t timeStamp_last = 0;
};

static bool loadFile(const char *path, const EventPoller::Ptr &poller) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        WarnL << "open eqq failed:" << path;
        return false;
    }
    fseek(fp, 0, SEEK_END);
    long lSize = ftell(fp);
    uint8_t *text = (uint8_t *)malloc(lSize);
    rewind(fp);
    fread(text, sizeof(char), lSize, fp);

    PsProcess::Ptr  ps_process = std::make_shared<PsProcess>();
    DecoderImp::Ptr ps_decoder = DecoderImp::createDecoder(DecoderImp::decoder_ps, ps_process.get());
    if (ps_decoder) {
        ps_decoder->input(text, lSize);
    }
    WarnL << (lSize >> 10) << "KB";
    fclose(fp);
    return true;
}

int main(int argc, char *argv[]) {
    // 设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>("ConsoleChannel"));

    // 启动异步日志线程
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
    loadIniConfig((exeDir() + "config.ini").data());

    TcpServer::Ptr rtspSrv(new TcpServer());
    TcpServer::Ptr rtmpSrv(new TcpServer());
    TcpServer::Ptr httpSrv(new TcpServer());
    rtspSrv->start<RtspSession>(554);  // 默认554
    rtmpSrv->start<RtmpSession>(1935); // 默认1935
    httpSrv->start<HttpSession>(81);   // 默认80

    if (argc == 2) {
        auto poller = EventPollerPool::Instance().getPoller();
        poller->async_first([poller, argv]() {
            loadFile(argv[1], poller);
            sem.post();
        });
        sem.wait();
        sleep(1);
    } else
        ErrorL << "parameter error.";
    return 0;
}