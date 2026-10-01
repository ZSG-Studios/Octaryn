"""Exercise real concurrent writers and preserve stderr after watchdog failures."""
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

from capture_watchdog import run_capture
from tile_performance_report import parse_tile_work


class CaptureOutputTests(unittest.TestCase):
    def test_concurrent_fragmented_streams_keep_metrics_and_errors_intact(self):
        code = '''import os,threading
gate=threading.Barrier(2)
def writer(fd,prefix):
 gate.wait()
 for i in range(64):
  record=(prefix+str(i)+"\\n").encode()
  for value in record: os.write(fd,bytes([value]))
a=threading.Thread(target=writer,args=(1,"tile_upload bytes=1 budget=512 cpu_ms="))
b=threading.Thread(target=writer,args=(2,"rhi_validation severity=error fixture="))
a.start();b.start();a.join();b.join()
'''
        with tempfile.TemporaryDirectory(prefix='capture-output-') as folder:
            case = Path(folder)
            with (case / 'client.log').open('wb') as log:
                self.assertEqual(run_capture([sys.executable, '-c', code], case, dict(os.environ), log, 5), 0)
            stderr = (case / 'client.stderr.log').read_bytes()
            expected = b''.join(f'rhi_validation severity=error fixture={i}\n'.encode() for i in range(64))
            self.assertEqual(stderr, expected)
            merged = (case / 'client.log').read_bytes()
            prefix, suffix = merged.split(b'\ncapture_stderr_append ordering=nonchronological source=client.stderr.log\n')
            self.assertEqual(suffix, stderr + b'\ncapture_stderr_append_end ordering=nonchronological\n')
            expected_stdout = [f'tile_upload bytes=1 budget=512 cpu_ms={i}' for i in range(64)]
            self.assertEqual([line for line in prefix.decode().splitlines() if line.startswith('tile_upload')],
                             expected_stdout)
            parsed = parse_tile_work(merged.decode())
            self.assertEqual(parsed['measurement_completeness']['marker_records']['tile_upload'], 64)
            self.assertEqual(merged.count(b'rhi_validation severity=error'), 64)

    def test_timeout_preserves_and_appends_raw_stderr_after_owned_teardown(self):
        code = 'import os,time;os.write(2,b"rhi_validation severity=error preserved\\xff\\n");time.sleep(60)'
        with tempfile.TemporaryDirectory(prefix='capture-output-timeout-') as folder:
            case = Path(folder)
            with (case / 'client.log').open('wb') as log:
                with self.assertRaisesRegex(RuntimeError, 'overall timeout'):
                    run_capture([sys.executable, '-c', code], case, dict(os.environ), log, 1)
            original = (case / 'client.stderr.log').read_bytes()
            self.assertEqual(original, b'rhi_validation severity=error preserved\xff\n')
            self.assertIn(original, (case / 'client.log').read_bytes())

    def test_failed_teardown_retains_stderr_without_reading_live_stream(self):
        def fail(command, case, env, log, stderr, closed, timeout, **options):
            stderr.write(b'preserved teardown diagnostic\n')
            raise RuntimeError('owned process teardown failed')
        with tempfile.TemporaryDirectory(prefix='capture-output-close-') as folder:
            case = Path(folder)
            with patch('capture_watchdog._run_capture', side_effect=fail), (case / 'client.log').open('wb') as log:
                with self.assertRaisesRegex(RuntimeError, 'teardown failed'):
                    run_capture([], case, {}, log, 1)
            self.assertEqual((case / 'client.stderr.log').read_bytes(), b'preserved teardown diagnostic\n')
            self.assertNotIn(b'capture_stderr_append', (case / 'client.log').read_bytes())


if __name__ == '__main__':
    unittest.main()
