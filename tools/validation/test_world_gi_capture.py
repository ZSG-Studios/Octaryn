"""Admission retries are acceptable only with current, complete GPU evidence."""
import copy
import unittest

from world_gi_capture import assert_world_gi


def fixture():
    return dict(gi_mode='block-transport', gi_admission='world_geometry',
        block_transport_active=True, block_transport_coverage_valid=True,
        ray_coverage_complete=True, ray_pending_columns=0, block_transport_ready=True,
        block_transport_admission_sweeps=4, block_transport_measured_admission_sweeps=4,
        block_transport_admission_clean=True, block_transport_pinned_rows=12,
        block_transport_statistics_frame=240, render_frame=240, block_transport_contributor_start_frame=200,
        block_transport_counters=[200, 30, 20, 5, 0, 0, 100, 100, 16, 16, 30, 4],
        block_transport_ready_rows=16, block_transport_selection_occupied=16,
        block_transport_gpu_bytes=1024, block_transport_total_gpu_bytes=2048)


class WorldGiCaptureTests(unittest.TestCase):
    def test_historical_retry_requires_new_clean_sweep(self):
        value = fixture()
        self.assertIs(assert_world_gi(value), value)
        for key, replacement in [('block_transport_admission_clean', False),
                ('block_transport_measured_admission_sweeps', 0),
                ('block_transport_contributor_start_frame', 241),
                ('block_transport_measured_admission_sweeps', 5),
                ('block_transport_pinned_rows', 0), ('block_transport_pinned_rows', 17)]:
            bad = copy.deepcopy(value)
            bad[key] = replacement
            with self.subTest(key=key, replacement=replacement), self.assertRaises(RuntimeError):
                assert_world_gi(bad)

    def test_no_stale_fence_or_uninitialized_row(self):
        for key in ('block_transport_statistics_frame', 'block_transport_ready_rows',
                    'block_transport_selection_occupied'):
            bad = fixture()
            bad[key] -= 1
            with self.subTest(key=key), self.assertRaises(RuntimeError):
                assert_world_gi(bad)
        bad = fixture()
        bad['block_transport_counters'][9] -= 1
        with self.assertRaises(RuntimeError):
            assert_world_gi(bad)

    def test_missing_new_proof_is_rejected(self):
        for key in ('block_transport_admission_clean', 'block_transport_measured_admission_sweeps',
                    'block_transport_pinned_rows', 'block_transport_contributor_start_frame'):
            bad = fixture()
            del bad[key]
            with self.subTest(key=key), self.assertRaises(RuntimeError):
                assert_world_gi(bad)


if __name__ == '__main__':
    unittest.main()
