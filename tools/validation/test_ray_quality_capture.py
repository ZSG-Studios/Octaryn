"""CPU contracts for isolated RT quality captures, without launching a client."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from capture_map_world import QUALITY_TIERS, quality_settings, validate_capture_schedule


class RayQualityCapture(unittest.TestCase):
    def test_each_quality_is_independent(self):
        for reflection, reflection_name in enumerate(QUALITY_TIERS):
            for shadow, shadow_name in enumerate(QUALITY_TIERS):
                self.assertEqual(quality_settings(reflection_name, shadow_name),
                                 dict(reflectionQuality=reflection, shadowQuality=shadow))

    def test_invalid_quality_rejected(self):
        with self.assertRaises(ValueError):
            quality_settings('invalid', 'high')

    def test_no_capture_before_map_warmup(self):
        with self.assertRaisesRegex(ValueError, 'warmup'):
            validate_capture_schedule(180, 120, 1, 16)
        validate_capture_schedule(181, 120, 1, 16)

    def test_all_requested_captures_must_fit(self):
        for first, captures, stride in ((300, 3, 16), (120, 3, 16), (300, 1, 1)):
            last = max(180, first) + (captures - 1) * stride
            with self.assertRaises(ValueError):
                validate_capture_schedule(last, first, captures, stride)
            validate_capture_schedule(last + 1, first, captures, stride)

    def test_default_schedule(self):
        validate_capture_schedule(360, 300, 3, 16)


if __name__ == '__main__':
    unittest.main()
