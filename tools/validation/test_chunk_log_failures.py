"""Diagnostic counts must not hide or invent explicit client failures."""
from pathlib import Path
import tempfile
import unittest

from benchmark_chunk_loading import failure_lines, summarize


# Actual corrected-object capture, frame 240; its nonzero admission count is retained.
STATISTICS = ('block_transport_statistics frame=240 measured_frame=238 measured_epoch=1 epoch=1 '
              'radiance_epoch=1 admission_attempts=856982 admission_failures=4708 surface_hits=854224 '
              'sky_hits=378873 unknown_hits=774559 unsupported_hits=80136 transport_rays=2087792 '
              'direct_rays=125869 occupied_rows=3849 initialized_rows=3784 row_refreshes=130487 '
              'resident_capacity_pressure=0 frames_since_reset=157 eviction_passes=0 gpu_bytes=35116304 '
              'scheduled_rows=2048 ready_rows=3683 selection_occupied=3849 selection_start=18697 '
              'selection_wraps=45 work_cumulative_since_reset=1 occupancy_live=1')


class ChunkLogFailures(unittest.TestCase):
    def test_numeric_diagnostic_counters_are_not_failure_markers(self):
        for count in (0, 4708):
            with self.subTest(count=count):
                line = STATISTICS.replace('admission_failures=4708', f'admission_failures={count}')
                self.assertEqual(failure_lines(line), [])
        self.assertEqual(failure_lines('probe failures=2 failure_count=2 failed_count=2 timeout=240'), [])

    def test_launch_status_markers_and_ordinary_errors_are_detected(self):
        lines = [
            'world_renderer_initialize_failed stage=device sdl_error=unavailable',
            'client_boot stage=renderer status=failed',
            'world_mesh_allocator retirement_query_failed result=-1',
            'Slang RHI world renderer initialization failed',
            'Unexpected device failure during submission',
            'client_boot_capture failed=readback path=frame.bmp',
        ]
        self.assertEqual(failure_lines('\n'.join(lines)), lines)

    def test_validation_errors_and_timeouts_are_detected(self):
        lines = [
            'Validation Error: invalid resource state',
            'rhi_validation severity=error message=invalid descriptor',
            'rhi_validation backend=D3D12 severity=error message=invalid descriptor',
            'World startup timed out: waiting for server',
            'Capture stopped: overall timeout',
        ]
        self.assertEqual(failure_lines('\n'.join(lines)), lines)
        self.assertEqual(failure_lines('rhi_validation severity=warning message=diagnostic'), [])

    def test_statistics_do_not_mask_a_real_error_on_the_same_line(self):
        line = STATISTICS + ' status=failed'
        self.assertEqual(failure_lines(line), [line])

    def test_summary_keeps_nonzero_statistics_for_analysis(self):
        with tempfile.TemporaryDirectory() as temporary:
            case = Path(temporary)
            (case / 'client.log').write_text(STATISTICS + '\n')
            result = summarize(case, 4)
            self.assertEqual(result['failures'], [])
            self.assertEqual(result['block_transport_statistics'], [STATISTICS])
            self.assertIn('admission_failures=4708', result['block_transport_statistics'][0])


if __name__ == '__main__':
    unittest.main()
