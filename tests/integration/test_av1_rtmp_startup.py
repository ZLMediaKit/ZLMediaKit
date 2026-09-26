#!/usr/bin/env python3
"""Offline startup regressions; no FFmpeg or MediaServer process required."""
import configparser
import io
from pathlib import Path
import socket
import tempfile
import unittest
from unittest.mock import Mock, patch

import av1_rtmp_metadata as integration


class StartupTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='zlm-startup-test-')
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.cfg = configparser.ConfigParser()
        self.cfg.read_dict({'rtmp': {'port': '0'}, 'http': {'port': '0'}, 'api': {'secret': 'test-secret'}})

    def test_both_ports_are_distinct_and_reserved(self):
        with integration.reserve_ports() as ports:
            self.assertNotEqual(*ports)
            for port in ports:
                with socket.socket() as contender:
                    with self.assertRaises(OSError):
                        contender.bind(('127.0.0.1', port))

    def test_failed_bind_retries_with_fresh_config(self):
        failed, healthy = Mock(), Mock()
        failed.poll.return_value = 1
        healthy.poll.return_value = None
        attempts = []

        def start(command, name):
            config = configparser.ConfigParser()
            config.read(command[command.index('-c') + 1])
            ports = [int(config[section]['port']) for section in ('rtmp', 'http')]
            self.assertNotEqual(*ports)
            # The reservations must have been released before spawning the server.
            for port in ports:
                with socket.socket() as listener:
                    listener.bind(('127.0.0.1', port))
            attempts.append((name, ports))
            return failed if len(attempts) == 1 else healthy

        with patch.object(integration.urllib.request, 'urlopen', return_value=io.BytesIO(b'{"code":0}')):
            rtmp, http, api = integration.start_server('MediaServer', self.cfg, self.work, start)
        self.assertEqual(len(attempts), 2)
        self.assertEqual([rtmp, http], attempts[-1][1])
        self.assertIn(':%d/' % http, api)
        failed.wait.assert_called_once()
        healthy.terminate.assert_not_called()

    def test_failed_readiness_stops_process_before_retry(self):
        failed, healthy = Mock(), Mock()
        failed.poll.return_value = healthy.poll.return_value = None
        start = Mock(side_effect=[failed, healthy])
        responses = [OSError('port not listening')] * 100 + [io.BytesIO(b'{"code":0}')]
        with patch.object(integration.urllib.request, 'urlopen', side_effect=responses), \
                patch.object(integration.time, 'sleep'):
            integration.start_server('MediaServer', self.cfg, self.work, start)
        self.assertEqual(start.call_count, 2)
        failed.terminate.assert_called_once()
        failed.wait.assert_called_once()
        healthy.terminate.assert_not_called()

    def test_startup_failure_is_bounded(self):
        failed = Mock()
        failed.poll.return_value = 1
        start = Mock(return_value=failed)
        with self.assertRaisesRegex(RuntimeError, 'after 3 attempts'):
            integration.start_server('MediaServer', self.cfg, self.work, start)
        self.assertEqual(start.call_count, 3)


if __name__ == '__main__':
    unittest.main()
