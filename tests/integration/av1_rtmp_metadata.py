#!/usr/bin/env python3
"""Local FFmpeg integration test; needs a DISABLE_REPORT=ON MediaServer build.

Runs two concurrent 1440p60 AV1/AAC publishers plus H.264 and HEVC controls.
The server binds only loopback; all generated media/config/logs live in a temp dir.
This supplements, but does not replace, OBS AMD AV1 + mpegts.js acceptance testing.
"""
import argparse
import configparser
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[2]


def free_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def run(args, **kwargs):
    return subprocess.run(args, check=True, timeout=90, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--all-intra', action='store_true', help='Use AV1 keyframes only to isolate unready-track timeout')
    args = parser.parse_args()
    server = args.server.resolve()
    results = {}
    processes = []
    logs = []
    with tempfile.TemporaryDirectory(prefix='zlm-av1-') as tmp:
        work = Path(tmp)
        rtmp_port, http_port = free_port(), free_port()
        cfg = configparser.ConfigParser(interpolation=None)
        cfg.optionxform = str
        cfg.read(ROOT / 'conf/config.ini')
        overrides = {
            'general': {'listen_ip': '127.0.0.1'},
            'api': {'secret': 'local-av1-regression'},
            'http': {'port': str(http_port), 'sslport': '0', 'rootPath': str(work)},
            'rtmp': {'port': str(rtmp_port), 'sslport': '0', 'directProxy': '1', 'enhanced': '1'},
            'rtsp': {'port': '0', 'sslport': '0'},
            'rtp_proxy': {'port': '0'},
            'rtc': {'port': '0', 'tcpPort': '0'},
            'srt': {'port': '0'},
            'shell': {'port': '0'},
            'protocol': {'continue_push_ms': '0', 'enable_audio': '1', 'add_mute_audio': '0',
                         'enable_rtmp': '1', 'enable_rtsp': '0', 'enable_hls': '0',
                         'enable_hls_fmp4': '0', 'enable_mp4': '0', 'enable_ts': '0', 'enable_fmp4': '0'},
        }
        for section, values in overrides.items():
            if section not in cfg:
                cfg.add_section(section)
            cfg[section].update(values)
        config_path = work / 'config.ini'
        with config_path.open('w') as out:
            cfg.write(out)

        def start(command, name):
            logfile = (work / (name + '.log')).open('w+')
            logs.append((name, logfile))
            process = subprocess.Popen(command, cwd=work, stdout=logfile, stderr=subprocess.STDOUT)
            processes.append(process)
            return process

        base_ffmpeg = ['ffmpeg', '-hide_banner', '-loglevel', 'error', '-nostdin']
        codecs = {
            'av1': ['-c:v', 'libaom-av1', '-cpu-used', '8', '-threads', '4', '-row-mt', '1', '-lag-in-frames', '0'],
            'h264': ['-c:v', 'libx264', '-preset', 'ultrafast', '-tune', 'zerolatency', '-threads', '4'],
            'hevc': ['-c:v', 'libx265', '-preset', 'ultrafast', '-x265-params', 'pools=2:frame-threads=2:log-level=error'],
        }
        try:
            for codec, options in codecs.items():
                size = '2560x1440' if codec == 'av1' else '640x360'
                run(base_ffmpeg + ['-f', 'lavfi', '-i', 'color=c=blue:size=' + size + ':rate=60',
                                  '-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=48000', '-t', '2']
                    + options + ['-g', '1' if args.all_intra and codec == 'av1' else '60', '-pix_fmt', 'yuv420p', '-c:a', 'aac', '-ac', '2', '-ar', '48000',
                                 '-f', 'mp4', str(work / (codec + '.mp4'))], stdout=subprocess.DEVNULL)
            run(base_ffmpeg + ['-i', str(work / 'av1.mp4'), '-c', 'copy', '-f', 'flv', str(work / 'av1.flv')])
            raw = (work / 'av1.flv').read_bytes()
            offset = 13
            packets = []
            while offset + 11 < len(raw) and len(packets) < 4:
                kind = raw[offset]
                size = int.from_bytes(raw[offset + 1:offset + 4], 'big')
                body = raw[offset + 11:offset + 11 + size]
                if kind == 9:
                    packets.append({'type': body[0] & 15, 'fourcc': body[1:5].decode('ascii'),
                                    'size': size, 'payload_hex': body[5:29].hex()})
                offset += size + 15
            results['av1_packets'] = packets
            print('AV1 packet prefixes:', packets, flush=True)
            srv = start([str(server), '-c', str(config_path), '-t', '2', '--affinity', '0',
                         '--log-dir', str(work / 'logs')], 'server')
            api = 'http://127.0.0.1:%d/index/api/getMediaList?secret=local-av1-regression' % http_port
            for _ in range(100):
                if srv.poll() is not None:
                    raise RuntimeError('MediaServer exited during startup')
                try:
                    with urllib.request.urlopen(api, timeout=1):
                        break
                except OSError:
                    time.sleep(0.1)
            else:
                raise RuntimeError('local API did not start')
            streams = {'av1-a': 'av1', 'av1-b': 'av1', 'h264': 'h264', 'hevc': 'hevc'}
            for name, codec in streams.items():
                start(base_ffmpeg + ['-re', '-stream_loop', '-1', '-i', str(work / (codec + '.mp4')),
                                     '-c', 'copy'] + (['-bsf:v', 'av1_metadata=td=' + ('insert' if name == 'av1-a' else 'remove')] if codec == 'av1' else [])
                      + ['-f', 'flv', 'rtmp://127.0.0.1:%d/live/%s' % (rtmp_port, name)], name)
            # Allow the baseline's unready-track timeout to surface audio-only results.
            time.sleep(12)
            with urllib.request.urlopen(api, timeout=3) as response:
                data = json.load(response)
            if data.get('code') != 0:
                raise RuntimeError('getMediaList failed: ' + str(data))
            media = {entry['stream']: entry for entry in data.get('data', []) if entry['schema'] == 'rtmp'}
            results['tracks'] = {name: media.get(name, {}).get('tracks', []) for name in streams}
            errors = []
            for name, codec in streams.items():
                tracks = results['tracks'][name]
                print(name, json.dumps(tracks), flush=True)
                video = next((t for t in tracks if t['codec_type'] == 0), {})
                audio = next((t for t in tracks if t['codec_type'] == 1), {})
                expected = (2560, 1440) if codec == 'av1' else (640, 360)
                if not (video.get('ready') and (video.get('width'), video.get('height')) == expected):
                    errors.append(name + ': missing/unready/wrong video dimensions')
                expected_codec = {'av1': 'AV1', 'h264': 'H264', 'hevc': 'H265'}[codec]
                if video.get('codec_id_name') != expected_codec:
                    errors.append(name + ': wrong/missing video codec')
                if codec == 'av1' and abs(video.get('fps', 0) - 60) > 1:
                    errors.append(name + ': FPS is not approximately 60')
                if not (audio.get('ready') and audio.get('sample_rate') == 48000 and audio.get('channels') == 2):
                    errors.append(name + ': missing/wrong AAC metadata')
                # Decode both tracks, so byte forwarding without playable video cannot pass.
                playback = subprocess.run(base_ffmpeg + ['-xerror', '-i', 'http://127.0.0.1:%d/live/%s.live.flv' % (http_port, name),
                                                         '-t', '1', '-map', '0:v:0', '-map', '0:a:0', '-f', 'null', '-'],
                                          capture_output=True, text=True, timeout=20)
                results.setdefault('playback', {})[name] = playback.returncode == 0
                if playback.returncode:
                    errors.append(name + ': HTTP-FLV decode failed: ' + playback.stderr)
            results['errors'] = errors
            if args.report:
                args.report.resolve().write_text(json.dumps(results, indent=2) + '\n')
            print('HTTP-FLV decoding:', results['playback'], flush=True)
            if errors:
                raise RuntimeError('; '.join(errors))
            print('PASS: two AV1 1440p60/AAC streams, H.264, HEVC, API metadata and HTTP-FLV decoding')
        except Exception:
            for name, logfile in logs:
                logfile.flush()
                logfile.seek(0)
                print(name + ' log tail:\n' + logfile.read()[-2500:])
            raise
        finally:
            for proc in reversed(processes):
                if proc.poll() is None:
                    proc.terminate()
            for proc in processes:
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()
            for name, logfile in logs:
                if args.report and name == 'server':
                    logfile.flush()
                    logfile.seek(0)
                    args.report.resolve().with_suffix('.server.log').write_text(logfile.read())
                logfile.close()


if __name__ == '__main__':
    main()
