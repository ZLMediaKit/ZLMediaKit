# ZLMediaKit Ultra - 音视频转码扩展模块

## 概述

本项目基于ZLMediaKit开源流媒体服务器，扩展实现了完整的音视频转码功能，支持多种编解码器之间的任意转换。

## 支持的编解码器

### 视频编解码器
| 编解码器 | 编码 | 解码 | 说明 |
|---------|------|------|------|
| H.264/AVC | ✅ | ✅ | 最广泛使用的视频编码 |
| H.265/HEVC | ✅ | ✅ | 高效视频编码 |
| VP8 | ✅ | ✅ | Google开发的开源编码 |
| VP9 | ✅ | ✅ | VP8的继任者 |
| AV1 | ✅ | ✅ | 下一代开源视频编码 |
| SVAC1 | ✅ | ✅ | 中国国标视频编码(基于H.264) |
| SVAC2 | ✅ | ✅ | 中国国标视频编码(类似H.265) |
| JPEG/MJPEG | ✅ | ✅ | Motion JPEG |

### 音频编解码器
| 编解码器 | 编码 | 解码 | 说明 |
|---------|------|------|------|
| AAC | ✅ | ✅ | 高级音频编码 |
| Opus | ✅ | ✅ | 低延迟音频编码 |
| G.711A | ✅ | ✅ | A-law PCM |
| G.711U | ✅ | ✅ | μ-law PCM |
| G.722 | ✅ | ✅ | 宽带语音编码 |
| G.722.1 | ✅ | ✅ | 宽带语音编码(Siren7) |
| MP3 | ✅ | ✅ | MPEG Audio Layer 3 |
| L16/PCM | ✅ | ✅ | 线性PCM |

## 编译要求

### 依赖项
- CMake 3.10+
- FFmpeg 4.0+ (需要启用以下库)
  - libavcodec
  - libavformat
  - libavutil
  - libswscale
  - libswresample
  - libavfilter
- 可选硬件加速
  - NVIDIA NVENC/NVDEC (需要CUDA)
  - Intel QSV (需要Intel Media SDK)
  - Apple VideoToolbox (macOS)

### 编译命令

```bash
# 创建构建目录
mkdir build && cd build

# 配置（启用FFmpeg支持）
cmake .. -DENABLE_FFMPEG=ON -DENABLE_TESTS=ON

# 编译
make -j$(nproc)
```

### macOS 编译

```bash
# 安装依赖
brew install cmake ffmpeg pkg-config

# 编译
mkdir build && cd build
cmake .. -DENABLE_FFMPEG=ON
make -j$(sysctl -n hw.ncpu)
```

### Ubuntu/Debian 编译

```bash
# 安装依赖
sudo apt-get update
sudo apt-get install -y cmake git gcc g++ \
    libffmpeg-dev libavcodec-dev libavformat-dev \
    libavutil-dev libswscale-dev libswresample-dev \
    libavfilter-dev

# 编译
mkdir build && cd build
cmake .. -DENABLE_FFMPEG=ON
make -j$(nproc)
```

## 使用方法

### 1. 命令行工具

```bash
# 基本用法
./test_transcode <源URL> [源编码] [目标编码]

# 示例：H.265转H.264
./test_transcode rtsp://192.168.1.100/stream h265 h264

# 示例：完整转码（视频+音频）
./test_transcode rtmp://192.168.1.100/live/stream h265:aac h264:opus
```

### 2. HTTP API

启动ZLMediaKit服务器后，可以通过HTTP API进行转码控制：

#### 启动转码任务
```bash
curl -X POST "http://127.0.0.1:80/api/transcode/start" \
  -d "src_url=rtsp://192.168.1.100/stream" \
  -d "video_codec=h264" \
  -d "audio_codec=aac" \
  -d "video_width=1280" \
  -d "video_height=720"
```

响应示例：
```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "task_id": "transcode_1_1703836800",
    "play_url": {
      "rtsp": "rtsp://127.0.0.1/transcode/transcode_1_1703836800",
      "rtmp": "rtmp://127.0.0.1/transcode/transcode_1_1703836800",
      "http_flv": "http://127.0.0.1/transcode/transcode_1_1703836800.flv",
      "hls": "http://127.0.0.1/transcode/transcode_1_1703836800/hls.m3u8"
    }
  }
}
```

#### 停止转码任务
```bash
curl -X POST "http://127.0.0.1:80/api/transcode/stop" \
  -d "task_id=transcode_1_1703836800"
```

#### 获取任务列表
```bash
curl "http://127.0.0.1:80/api/transcode/list"
```

#### 获取支持的编解码器
```bash
curl "http://127.0.0.1:80/api/transcode/codecs"
```

### 3. 代码集成

```cpp
#include "Codec/TranscodeAPI.h"

using namespace mediakit;

// 创建转码配置
TranscodeConfig config;
config.video_codec = CodecH264;
config.audio_codec = CodecAAC;
config.video_width = 1280;
config.video_height = 720;

// 创建转码器
auto transcoder = TranscodeAPI::create(config);

// 设置源
transcoder->setSource("rtsp://192.168.1.100/stream");

// 设置回调
transcoder->setOnFrame([](const Frame::Ptr &frame) {
    // 处理转码后的帧
});

// 注册为MediaSource
transcoder->regist("__defaultVhost__", "transcode", "output");

// 启动转码
transcoder->start();
```

## API参数说明

### 视频参数
| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| video_codec | string | h264 | 目标视频编码 |
| video_width | int | 保持原始 | 目标宽度 |
| video_height | int | 保持原始 | 目标高度 |
| video_fps | int | 保持原始 | 目标帧率 |
| video_bitrate | int | 2000000 | 视频比特率(bps) |

### 音频参数
| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| audio_codec | string | aac | 目标音频编码 |
| audio_sample_rate | int | 保持原始 | 采样率 |
| audio_channels | int | 保持原始 | 通道数 |
| audio_bitrate | int | 128000 | 音频比特率(bps) |

## 硬件加速

### NVIDIA GPU加速 (NVENC/NVDEC)
```bash
# 确保安装了NVIDIA驱动和CUDA
nvidia-smi

# 编译时会自动检测并启用
cmake .. -DENABLE_FFMPEG=ON
```

### Intel QSV加速
```bash
# 安装Intel Media SDK
sudo apt-get install libmfx-dev

# 编译
cmake .. -DENABLE_FFMPEG=ON
```

### Apple VideoToolbox (macOS)
macOS系统会自动启用VideoToolbox硬件加速。

## 性能建议

1. **硬件加速**: 优先使用GPU硬件加速（NVENC/NVDEC/QSV/VideoToolbox）
2. **编码预设**: 对于实时转码，使用`ultrafast`预设
3. **GOP设置**: 设置合理的关键帧间隔（如2秒一个GOP）
4. **多线程**: 根据CPU核心数设置编码线程数
5. **分辨率**: 按需降低分辨率可显著降低CPU负载

## 常见问题

### Q: 找不到FFmpeg库
A: 确保正确安装了FFmpeg开发库，并设置PKG_CONFIG_PATH环境变量。

### Q: 硬件加速不工作
A: 检查驱动是否正确安装，使用`ffmpeg -encoders`查看可用的硬件编码器。

### Q: 转码延迟高
A: 使用`ultrafast`预设，关闭B帧，减少缓冲区大小。

## 许可证

MIT License

## 贡献

欢迎提交Issue和Pull Request！

## 相关链接

- [ZLMediaKit官方项目](https://github.com/ZLMediaKit/ZLMediaKit)
- [FFmpeg官方文档](https://ffmpeg.org/documentation.html)
