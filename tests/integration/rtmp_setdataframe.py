#!/usr/bin/env python3
"""Loopback regression for #4873. Requires librtmp, ffprobe, FFmpeg with libx264, and a DISABLE_REPORT=ON MediaServer.

Run: python3 tests/integration/rtmp_setdataframe.py --server /path/to/MediaServer
"""
import argparse
import configparser
from contextlib import contextmanager, ExitStack
import ctypes
from ctypes.util import find_library
import json
from pathlib import Path
import secrets
import struct
import subprocess
import tempfile
import time
from urllib.request import urlopen

from av1_rtmp_metadata import start_server


@contextmanager
def publish(url):
    # RTMP_ConnectStream in write mode waits for NetStream.Publish.Start.
    # RTMP_Write takes FLV tags and prepends the AMF string "@setDataFrame"
    # to each script-data tag. Keeping the handle open lets HTTP clients join.
    library = find_library('rtmp')
    if not library:
        raise RuntimeError('librtmp is required')
    lib = ctypes.CDLL(library)
    lib.RTMP_Alloc.restype = ctypes.c_void_p
    handle = ctypes.c_void_p(lib.RTMP_Alloc())
    if not handle:
        raise MemoryError('RTMP_Alloc failed')
    lib.RTMP_Init(handle)
    address = ctypes.create_string_buffer((url + ' timeout=5').encode())
    try:
        if not lib.RTMP_SetupURL(handle, address):
            raise RuntimeError('RTMP_SetupURL failed')
        lib.RTMP_EnableWrite(handle)
        if not lib.RTMP_Connect(handle, None) or not lib.RTMP_ConnectStream(handle, 0):
            raise RuntimeError('RTMP publish failed: ' + url)

        def send(tags):
            return lib.RTMP_Write(handle, tags, len(tags)) == len(tags)

        yield send
    finally:
        lib.RTMP_Close(handle)
        lib.RTMP_Free(handle)


def metadata_tag(name, marked):
    fields = {'width': 704, 'height': 480, 'framerate': 30}
    if marked:
        fields['regression_id'] = 4873
    body = b'\x02' + struct.pack('>H', len(name)) + name + b'\x03'
    for key, number in fields.items():
        body += struct.pack('>H', len(key)) + key.encode() + b'\x00' + struct.pack('>d', number)
    body += b'\x00\x00\x09'
    header = b'\x12' + len(body).to_bytes(3, 'big') + bytes(7)
    return header + body + struct.pack('>I', len(header) + len(body))


def check_playback(url, marked):
    # Bound the live read and require decoded pictures, not just an advertised codec.
    result = subprocess.check_output([
        'ffprobe', '-v', 'error', '-analyzeduration', '1000', '-probesize', '1024',
        '-read_intervals', '%+#6', '-select_streams', 'v:0', '-show_entries',
        'frame=width,height,pict_type:format_tags=regression_id', '-of', 'json', url], timeout=10)
    data = json.loads(result)
    frames = data.get('frames', [])
    if len(frames) != 6 or any((f['width'], f['height']) != (704, 480) for f in frames):
        raise AssertionError(f'expected six decoded 704x480 frames: {frames}')
    if marked and data['format'].get('tags', {}).get('regression_id') != '4873':
        raise AssertionError('published metadata marker missing')


def ready_h264(api, stream):
    with urlopen(api + '&schema=rtmp&app=live&stream=' + stream, timeout=2) as response:
        listing = json.load(response)
    if listing['code'] != 0:
        raise RuntimeError(f'getMediaList failed: {listing}')
    return any(track['ready'] and track['codec_id_name'] == 'H264'
               and (track['width'], track['height']) == (704, 480)
               for entry in listing.get('data', []) for track in entry['tracks'])


def check_case(name, stream, video, rtmp, http, api, log, *, rejected=False, marked=False):
    with publish(f'rtmp://127.0.0.1:{rtmp}/live/{stream}') as send:
        if not send(metadata_tag(name, marked)):
            raise RuntimeError('metadata write failed: ' + stream)
        # A baseline server can close the connection before RTMP_Write finishes
        # sending video. Read the server's reason in both timing orders.
        sent_video = not rejected and send(video)
        for _ in range(50):
            lines = log.read_text(errors='replace').splitlines()
            if any('/live/' + stream + ')' in line and 'can only set metadata' in line for line in lines):
                if rejected:
                    print(f'PASS {name.decode()}: rejected with "can only set metadata"', flush=True)
                    return
                raise AssertionError(f'{name.decode()}: rejected with "can only set metadata"')
            if sent_video and ready_h264(api, stream):
                check_playback(f'http://127.0.0.1:{http}/live/{stream}.live.flv', marked)
                print(f'PASS {name.decode()}: ready H264 704x480, decoded 6 HTTP-FLV frames'
                      + (', metadata marker preserved' if marked else ''), flush=True)
                return
            time.sleep(0.1)
        expected = 'rejection' if rejected else 'ready H264 track'
        raise AssertionError(f'{name.decode()}: timed out waiting for {expected}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', required=True, type=Path)
    server = parser.parse_args().server.resolve()
    flv = subprocess.check_output([
        'ffmpeg', '-v', 'error', '-f', 'lavfi', '-i', 'testsrc2=s=704x480:r=30', '-frames:v', '30',
        '-c:v', 'libx264', '-profile:v', 'main', '-bf', '0', '-threads', '2', '-g', '60',
        '-flvflags', 'no_metadata+no_sequence_end', '-f', 'flv', '-'], timeout=30)
    video = flv[13:]  # Drop the FLV file header and initial PreviousTagSize.
    if video[0] != 9 or video[11:13] != b'\x17\x00':
        raise AssertionError('expected AVC sequence header as first video tag')

    with tempfile.TemporaryDirectory(prefix='zlm-setdataframe-') as tmp, ExitStack() as stack:
        work = Path(tmp)
        cfg = configparser.ConfigParser()
        cfg.optionxform = str
        cfg.read_dict({
            'api': {'secret': secrets.token_hex(16), 'apiDebug': '0'}, 'general': {'listen_ip': '127.0.0.1'},
            'rtmp': {'port': '0', 'sslport': '0', 'directProxy': '1'},
            'http': {'port': '0', 'sslport': '0', 'rootPath': tmp},
            'protocol': {'enable_audio': '0', 'continue_push_ms': '0', 'enable_rtsp': '0',
                         'enable_hls': '0', 'enable_fmp4': '0', 'enable_ts': '0'},
            'rtsp': {'port': '0', 'sslport': '0'}, 'rtc': {'port': '0', 'tcpPort': '0'},
            'rtp_proxy': {'port': '0'}, 'srt': {'port': '0'}, 'shell': {'port': '0'}, 'onvif': {'port': '0'},
        })
        log = work / 'server.log'
        output = stack.enter_context(log.open('wb'))

        def start(command, name):
            process = stack.enter_context(subprocess.Popen(command, cwd=work, stdout=output, stderr=subprocess.STDOUT))
            stack.callback(process.kill)
            return process

        try:
            rtmp, http, api = start_server(server, cfg, work, start)
            args = video, rtmp, http, api, log
            check_case(b'onMetaData', 'standard', *args, marked=True)
            check_case(b'@@onMetaData', 'near-match', *args, rejected=True)
            check_case(b'onCuePoint', 'other', *args, rejected=True)
            check_case(b'@onMetaData', 'dahua', *args)
            check_case(b'@onMetaData', 'marker', *args, marked=True)
        except Exception:
            print(log.read_text(errors='replace')[-4000:])
            raise


if __name__ == '__main__':
    main()
