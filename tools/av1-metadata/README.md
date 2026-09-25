# Enhanced RTMP AV1 metadata regression

Related issue: [ZLMediaKit/ZLMediaKit#4839](https://github.com/ZLMediaKit/ZLMediaKit/issues/4839).
The reproduction baseline is `10268396b89748bc2b0d19dda79bc29219e97815`, with
`3rdpart/media-server` at `21c4451ff2e4c4bb1c817e606c8b4e5deac1e719`.

## Cause and correction

Enhanced AV1/VP8/VP9 `CodedFrames` starts immediately after FourCC, without the
three-byte composition offset used by AVC/HEVC/VVC. The old shared VPx decoder
consumed the first three encoded bytes as that offset. Depending on the valid
OBU layout, it produced garbage dimensions, left the video track unready until
MediaSink discarded it, or rejected a short frame. `rtmp.directProxy=1` can still
forward the original playable packets while internal track metadata is wrong.

The patch preserves the coded bytes, fixes the matching encoder format and
packet length, and continues accepting the old ZLM `CodedFramesX` layout.
Classic domestic-extension packets retain their composition offsets.

The pinned `aom_av1_codec_configuration_record_load()` copies av1C/configOBUs;
it does not populate dimensions. AV1Track now explicitly parses configOBUs.
Frame parsing uses a fresh context and only commits a successful sequence header,
so repeated headers cannot fill the 2 KiB configuration buffer and failed/inter
frames cannot overwrite known dimensions. The pinned submodule is unchanged.

The constant 30 FPS was the inherited VideoTrackImp default, not an observed
rate. AV1 now starts with unknown FPS, and RtmpDemuxer carries the publisher's
finite positive `onMetaData.framerate` into AV1/VPx tracks before cloning. Missing
or invalid metadata stays unknown for AV1; this is not a general AV1 timing-info
parser or a new timestamp-based estimator. H.264/HEVC retain their bitstream FPS.

The premature video discovery flag is also removed. A separate regression proves
that an unsupported first packet no longer prevents a later valid AV1 packet
from creating a track. This recovery defect is distinct from the reproduced
unready-track timeout; an unsupported first OBS packet has not been established.

Primary references:

- [Enhanced RTMP video layout](https://veovera.org/docs/enhanced/enhanced-rtmp-v2.html#enhanced-video)
- [OBS 32.2.2 FLV muxer](https://github.com/obsproject/obs-studio/blob/32.2.2/plugins/obs-outputs/flv-mux.c):
  `flv_packet_ex()` only writes CTS for H.264/HEVC; `build_flv_meta_data()` writes frame rate.
- [Pinned AV1 parser](https://github.com/ireader/media-server/blob/21c4451ff2e4c4bb1c817e606c8b4e5deac1e719/libflv/source/aom-av1.c).

## Native local build and tests

Run from the repository root in WSL2. Keep the production checkout separate.

```sh
git submodule update --init --recursive
cmake -S . -B build-av1 -DCMAKE_BUILD_TYPE=Debug -DDISABLE_REPORT=ON -DENABLE_TESTS=ON
cmake --build build-av1 --target MediaServer test_av1_rtmp test_amf test_sortor -j 12
ctest --test-dir build-av1 --output-on-failure
```

`test_av1_rtmp` embeds a tiny independently encoded 2560x1440 libaom fixture.
It checks discovery recovery, av1C parsing and round trips, 200 repeated sequence
headers, failed-frame recovery, packet bytes/timestamps, 60 and 59.94 FPS and
cloning, invalid FPS, short AV1/VP8/VP9 packets, encoder bytes, classic CTS, and
older ZLM `CodedFramesX` compatibility. It requires no network or external files.

The optional integration test needs Python 3 and FFmpeg with libaom-av1, libx264,
libx265, AAC, Enhanced FLV, and the `av1_metadata` bitstream filter:

```sh
python3 tests/integration/av1_rtmp_metadata.py \
  --server release/linux/Debug/MediaServer --report /tmp/av1-inter.json
python3 tests/integration/av1_rtmp_metadata.py \
  --server release/linux/Debug/MediaServer --all-intra --report /tmp/av1-intra.json
```

It binds the server to loopback on temporary ports and runs two simultaneous
2560x1440@60 AV1/AAC publishers, one with temporal delimiters and one without,
plus H.264/HEVC controls. It checks `getMediaList` and decodes both HTTP-FLV audio
and video with FFmpeg. Generated media/configs and child processes are cleaned up;
`--report` retains JSON results and a sibling `.server.log` file.

Observed with the all-intra fixture on the pinned baseline versus the patch:

| Stream | Baseline API | Patched API | HTTP-FLV decode, both versions |
|---|---|---|---|
| AV1 with delimiter | AV1 1005x64406, 30 FPS | AV1 2560x1440, 60 FPS, ready | Pass |
| AV1 without delimiter | AAC only after 10-second track timeout | AV1 2560x1440, 60 FPS, ready | Pass |
| H.264/AAC | 640x360, 60 FPS | 640x360, 60 FPS | Pass |
| HEVC/AAC | 640x360, 60 FPS | 640x360, 60 FPS | Pass |

The inter-frame case also passes after the patch. Its delimiter-free baseline
can disconnect on a short AV1 payload. These generated fixtures establish the
code defects; the exact reported `1x235` value and the original AMD packets have
not been captured locally.

## Optional finite diagnostics

The normal patch does not log packet contents. To instrument a local test build:

```sh
git apply --unidiff-zero --check tools/av1-metadata/diagnostics.patch
git apply --unidiff-zero tools/av1-metadata/diagnostics.patch
cmake --build build-av1 --target MediaServer -j 12
```

`AV1_DIAG` lines capture the first 16 packets per demuxer/AV1 decoder, first 3
configurations per AV1 track, and first 12 frames per AV1 track. They include
packet type/FourCC/sizes, up to 32 initial payload bytes, parser return values and
dimensions, DTS/PTS, metadata FPS, track FPS, and GOP values used by the API's
fallback. Track pointers distinguish cloned tracks. Logging stops at those caps;
direct proxy may stop demuxing earlier once all tracks are ready.

After collecting logs:

```sh
git apply --unidiff-zero -R tools/av1-metadata/diagnostics.patch
cmake --build build-av1 --target MediaServer -j 12
```

## Local Docker build

Use the existing Docker recipe with only the requested reporting option added.
The stdin Dockerfile keeps the repository's normal recipe unchanged:

```sh
sed 's/RUN cmake /RUN cmake -DDISABLE_REPORT=ON /' dockerfile | \
  docker build -f - -t zlmediakit:av1-metadata .
docker run --rm --name zlm-av1-local \
  -p 127.0.0.1:19350:1935 -p 127.0.0.1:18080:80 \
  zlmediakit:av1-metadata
```

The native builds and tests above were run with `DISABLE_REPORT=ON`. The Docker
image was not built in this workspace because no Docker daemon was running.

Before production rollout, test two real OBS AMD AV1 2560x1440@60 publishers with
`directProxy=1`, `enhanced=1`, `continue_push_ms=0`, audio and RTMP enabled. Confirm
both ready AV1 tracks, dimensions and approximately 60 FPS, AAC, mpegts.js browser
playback, and H.264/HEVC controls. The local FFmpeg tests do not substitute for
that hardware-encoder/browser acceptance test.
