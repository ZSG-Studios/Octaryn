"""Reject corrupted/mismatched GPU evidence and preserve signed exact surface keys."""
import math
from pathlib import Path
import struct
import tempfile
import unittest
from block_transport_values import MAGIC, compare, load, summarize


def snapshot(frame=9, epoch=5, energy=2):
    header = MAGIC + struct.pack('<8I', 1, 2, 3, epoch, frame, 0, 64, 16)
    surface = struct.pack('<3iI4f8I', -12, -100, 42, 3, .5, .6, .7, 1, 3, 8, frame, 8, 17, 0, 0, 0)
    value = struct.pack('<3fI', energy, 0, 1, epoch) + bytes(16)
    return header + surface + bytes(64) + value * 3


class CacheEvidenceTests(unittest.TestCase):
    def test_exact_linear_roundtrip_and_frame_match(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / 'cache.bin'; path.write_bytes(snapshot())
            result = load(path)
            self.assertEqual(result['rows'][-12, -100, 42, 3]['indirect'], [2, 0, 1])
            capture = dict(render_frame=9, block_transport_epoch=3, block_transport_radiance_epoch=5,
                           block_transport_counters=[0] * 8 + [1, 1, 1, 0], block_transport_ready=True)
            self.assertEqual(summarize(path, capture)['batch_min'], 8)
            capture['render_frame'] = 10
            with self.assertRaisesRegex(ValueError, 'epochs differ'):
                summarize(path, capture)

    def test_corruption_and_nonfinite_fail_closed(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / 'cache.bin'
            for data in (snapshot()[:-1], snapshot() + b'x', b'wrong' + snapshot()[5:], snapshot(energy=math.nan)):
                path.write_bytes(data)
                with self.assertRaises(ValueError):
                    load(path)

    def test_temporal_linear_difference_and_epoch_rejection(self):
        with tempfile.TemporaryDirectory() as root:
            a, b = Path(root) / 'a', Path(root) / 'b'
            a.write_bytes(snapshot()); b.write_bytes(snapshot(frame=10, energy=3))
            difference = compare(a, b)
            self.assertAlmostEqual(difference['indirect']['rgb_rmse'], math.sqrt(1 / 3))
            b.write_bytes(snapshot(frame=10, epoch=6))
            with self.assertRaisesRegex(ValueError, 'epochs differ'):
                compare(a, b)


if __name__ == '__main__':
    unittest.main()
