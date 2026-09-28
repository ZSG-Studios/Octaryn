"""Reject unactivated or contaminated same-binary loading comparisons."""
import unittest
from capture_tile_world import texture_reuse_evidence


class ReuseEvidenceTests(unittest.TestCase):
    def test_activation_and_counters(self):
        prefix = 'tile_texture_reuse enabled=1 publication=fence_complete cache_namespace=world_immutable\n'
        counters = 'tile_stream frame=1 texture_reuses=12 avoided_dds_bytes=1024\n'
        result = texture_reuse_evidence(prefix + counters, 'on')
        self.assertEqual(result['completed_asset_reuses'], 12)
        self.assertEqual(result['avoided_dds_payload_bytes'], 1024)
        for text, mode in [(counters, 'on'), (prefix, 'on'), (prefix+counters, 'off'),
                           (prefix.replace('enabled=1','enabled=0')+counters, 'off')]:
            with self.subTest(mode=mode,text=text), self.assertRaises(ValueError):
                texture_reuse_evidence(text,mode)
        baseline = prefix.replace('enabled=1','enabled=0') + counters.replace('12','0').replace('1024','0')
        self.assertFalse(texture_reuse_evidence(baseline,'off')['enabled'])


if __name__ == '__main__':
    unittest.main()
