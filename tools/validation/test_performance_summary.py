"""Profiling joins must reject shifted columns and exclude capture stalls."""
import csv
import json
from pathlib import Path
import tempfile
import unittest

from performance_summary import read_profile, summarize_case, distribution


class PerformanceEvidence(unittest.TestCase):
    def test_schema_rejects_old_and_shifted_records(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'gpu.csv'
            for content in ('frame,total_gpu_ms\n1,3\n',
                            'frame,total_gpu_ms,schema_version\n1,3,2,2560\n',
                            'frame,total_gpu_ms,schema_version\n1,nan,2\n',
                            'frame,total_gpu_ms,schema_version\n1,3,2\n1,4,2\n'):
                path.write_text(content)
                with self.subTest(content=content), self.assertRaises(ValueError):
                    read_profile(path)

    def test_joins_completed_frames_and_excludes_capture(self):
        with tempfile.TemporaryDirectory() as directory:
            case = Path(directory)
            for name, column in [('gpu.csv', 'total_gpu_ms'), ('lighting.csv', 'reflection_trace_ms'),
                                 ('frame-timing.csv', 'total_ms')]:
                with (case / name).open('w', newline='') as stream:
                    writer = csv.writer(stream)
                    writer.writerow(['frame', column, 'schema_version'])
                    for frame in range(10, 30):
                        writer.writerow([frame, 1000 if frame == 20 else 5, 2])
            (case / 'frame.bmp.observation.json').write_text(json.dumps({'frame': 20}))
            report = summarize_case(case, warmup=2)
            self.assertEqual(report['measured_frames'], 14)
            self.assertEqual(report['excluded_capture_frames'], [19, 20, 21, 22])
            self.assertEqual(report['measured']['cpu']['total_ms']['worst'], 5)
            self.assertEqual(report['frame_range'], [12, 29])

    def test_schema4_preserves_exclusive_fields(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'lighting.csv'
            path.write_text('frame,reflection_screen_ms,reflection_coverage_ms,clouds_ms,map_forward_ms,reactive_copy_ms,schema_version\n1,.1,.2,.3,.4,.5,4\n')
            record = read_profile(path)[1]
            self.assertEqual(record['reflection_screen_ms'], .1)
            self.assertEqual(record['map_forward_ms'], .4)

    def test_percentiles_retain_tail(self):
        result = distribution([1, 1, 1, 1, 100])
        self.assertEqual(result['median'], 1)
        self.assertEqual(result['worst'], 100)
        self.assertGreater(result['p99'], result['p95'])


if __name__ == '__main__':
    unittest.main()
