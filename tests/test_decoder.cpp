
#include "Codec/Transcode.h"
#include "Common/config.h"
#include "Player/MediaPlayer.h"
#include "Rtsp/UDPServer.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include <iostream>
#include <list>
#include <mutex>
#include <signal.h>

using namespace std;
using namespace toolkit;
using namespace mediakit;

std::mutex mx;
std::list<mediakit::FFmpegFrame::Ptr> g_list;

class TestPlayer : public std::enable_shared_from_this<TestPlayer> {
public:
    using Ptr = std::shared_ptr<TestPlayer>;

    TestPlayer(const std::string &url)
        : _url(url) {}

    void play() // 遍历，然后调用channel的回调
    {
        auto url = _url;
        // 创建拉流 解码对象
        auto player = std::make_shared<mediakit::MediaPlayer>();
        std::weak_ptr<mediakit::MediaPlayer> weakPlayer = player;

        std::weak_ptr<TestPlayer> weakSelf = shared_from_this();

        player->setOnPlayResult([weakPlayer, weakSelf, url](const toolkit::SockException &ex) mutable {
            InfoL << "OnPlayResult:" << ex.what();
            auto strongPlayer = weakPlayer.lock();
            if (!strongPlayer) {
                return;
            }

            auto videoTrack = std::dynamic_pointer_cast<mediakit::VideoTrack>(strongPlayer->getTrack(mediakit::TrackVideo, false));
            // auto audioTrack = std::dynamic_pointer_cast<mediakit::AudioTrack>(strongPlayer->getTrack(mediakit::TrackAudio, false));

            if (videoTrack) {
                // auto decoder = std::make_shared<mediakit::FFmpegDecoder>(videoTrack);
                auto decoder = std::make_shared<mediakit::FFmpegDecoder>(videoTrack, 0, std::vector<std::string> { "h264", "hevc" });
                decoder->setOnDecode([weakSelf](const mediakit::FFmpegFrame::Ptr &frame) mutable {
                    auto strongSelf = weakSelf.lock();
                    if (!strongSelf) {
                        return;
                    }

                    // strongSelf->frame_list.push_back(frame);
                    std::unique_lock<std::mutex> lock(mx);
                    g_list.push_back(frame);
                    InfoL << "pts:" << frame->get()->pts << "size: " << g_list.size();
                });

                videoTrack->addDelegate((std::function<bool(const mediakit::Frame::Ptr &)>)[ weakSelf, decoder ](const mediakit::Frame::Ptr &frame) {
                    return decoder->inputFrame(frame, false, false);
                });
            }
        });

        player->setOnShutdown([weakPlayer, url, weakSelf](const toolkit::SockException &ex) {
            WarnL << "play shutdown: " << ex.what();
            auto strongPlayer = weakPlayer.lock();
            if (!strongPlayer) {
                return;
            }
        });

        (*player)[mediakit::Client::kWaitTrackReady] = false; // 不等待TrackReady
        (*player)[mediakit::Client::kRtpType] = mediakit::Rtsp::RTP_TCP;

        player->play(url);

        _player = player;
    }

public:
    std::list<mediakit::FFmpegFrame::Ptr> frame_list;

private:
    std::string _url;

    mediakit::MediaPlayer::Ptr _player;
};

int main() {
    for (int i = 0; i < 100; i++) {
        {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            auto player1 = std::make_shared<TestPlayer>("rtsp://kkem.me:1554/live/test");
            auto player2 = std::make_shared<TestPlayer>("rtsp://kkem.me:1554/live/test");
            auto player3 = std::make_shared<TestPlayer>("rtsp://kkem.me:1554/live/test");

            player1->play();
            player2->play();
            player3->play();

            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        std::unique_lock<std::mutex> lock(mx);
        g_list.clear();
    }
    getchar();
    return 0;
}