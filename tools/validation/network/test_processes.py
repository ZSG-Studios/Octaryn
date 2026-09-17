"""Exercise graceful/forced cleanup against owned Python fixtures, not engine apps."""
import os
from pathlib import Path
import sys
import tempfile
import unittest

from processes import launch, stop_child


ROOT = Path(__file__).resolve().parents[3]
WAIT_MARKER = '''import pathlib,sys,time
marker = pathlib.Path(sys.argv[1])
while not marker.exists():
    time.sleep(.01)
'''


class ProcessTests(unittest.TestCase):
    def test_marker_shutdown_and_timeout_only_target_owned_child(self):
        preset = 'release-windows' if os.name == 'nt' else 'release-linux'
        parent = ROOT / 'build' / preset / 'tools/validation'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='process-unit-', dir=parent) as directory:
            work = Path(directory)
            first = second = None
            with (work / 'children.log').open('w') as output:
                try:
                    marker = work / 'graceful.shutdown.request'
                    first = launch([sys.executable, '-B', '-c', WAIT_MARKER, str(marker)],
                                   work, os.environ.copy(), output)
                    second = launch([sys.executable, '-B', '-c', 'import time; time.sleep(60)'],
                                    work, os.environ.copy(), output)
                    forced_marker = work / 'forced.shutdown.request'
                    forced = stop_child(second, forced_marker, .01)
                    self.assertEqual(forced['pid'], second.pid)
                    self.assertTrue(forced['terminate'])
                    self.assertTrue(forced_marker.is_file())
                    self.assertIsNotNone(second.returncode)
                    self.assertIsNone(first.poll(), 'cleanup terminated the unrelated sibling')
                    graceful = stop_child(first, marker, 3)
                    self.assertEqual(graceful['exit_code'], 0)
                    self.assertFalse(graceful['terminate'])
                    self.assertFalse(graceful['kill'])
                    self.assertTrue(marker.is_file())
                finally:
                    for child, name in ((first, 'graceful'), (second, 'forced')):
                        if child is not None and child.poll() is None:
                            stop_child(child, work / (name + '.shutdown.request'), 1)


if __name__ == '__main__':
    unittest.main()
