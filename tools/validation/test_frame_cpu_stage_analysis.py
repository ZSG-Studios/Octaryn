"""Protect exact thread/clock attribution and nonadditive timing interpretation."""
import unittest
import json
from pathlib import Path
import tempfile
from analyze_frame_cpu_stages import analyze, frame_rows, join_stage, records, union_ns


class FrameCpuStageAnalysis(unittest.TestCase):
    def test_nested_backend_timers_count_once(self):
        self.assertEqual(union_ns([(10, 50), (20, 40), (50, 60), (80, 90)]), 60)

    def test_threads_and_partial_overlap_are_kept_distinct(self):
        stage = dict(thread_id=7, start_ns=10_000_000, end_ns=30_000_000, wall_ms=20)
        calls = [dict(thread_id=7, start_ns=12_000_000, end_ns=28_000_000),
                 dict(thread_id=7, start_ns=20_000_000, end_ns=40_000_000),
                 dict(thread_id=8, start_ns=10_000_000, end_ns=30_000_000)]
        result = join_stage(stage, calls)
        self.assertEqual(len(result['native_calls']), 2)
        self.assertEqual(result['native_interval_union_ms'], 18)
        self.assertEqual(result['outside_logged_native_intervals_ms'], 2)
        self.assertEqual([row['contained'] for row in result['native_calls']], [True, False])

    def test_real_format_preserves_epoch_and_frame_identity(self):
        text = ('world_frame_cpu_stage renderer_frame=18 app_frame=19 stage=command_finish '
                'thread_id=73 start_ns=475026992573900 end_ns=475027007085500 '
                'wall_ms=14.511600 thread_cpu_ns=0 thread_cpu_ms=0 end=boundary\n'
                'rhi_d3d12_timing op=command_record thread_id=73 start_ns=475026992573900 '
                'end_ns=475027007085500 wall_ms=14.511600 thread_cpu_ms=0 count=0 bytes=0 heap=0\n')
        stages, native = records(text)
        self.assertEqual(stages[0]['renderer_frame'], 18)
        self.assertEqual(stages[0]['app_frame'], 19)
        self.assertEqual(join_stage(stages[0], native)['native_interval_union_ms'], 14.5116)

    def test_corrupt_clock_record_is_rejected(self):
        with self.assertRaises(ValueError):
            records('rhi_d3d12_timing op=test thread_id=1 start_ns=10 end_ns=20 wall_ms=99')

    def test_large_absolute_clock_keeps_integer_nanosecond_precision(self):
        start = 10 ** 18 + 3
        stage = dict(thread_id=7, start_ns=start, end_ns=start + 12_345_678, wall_ms=12.345678)
        calls = [dict(thread_id=7, start_ns=start + 1, end_ns=start + 12_345_677),
                 dict(thread_id=7, start_ns=3, end_ns=12_345_681)]
        result = join_stage(stage, calls)
        self.assertEqual(result['native_interval_union_ms'], 12.345676)
        self.assertEqual(len(result['native_calls']), 1)

    def test_concatenated_log_records_do_not_overwrite_thread_or_clock(self):
        text = ('rhi_d3d12_timing op=first thread_id=1 start_ns=10 end_ns=20 wall_ms=0.000010 '
                'rhi_d3d12_timing op=second thread_id=2 start_ns=30 end_ns=50 wall_ms=0.000020\n')
        _, calls = records(text)
        self.assertEqual([(row['op'], row['thread_id'], row['start_ns']) for row in calls],
                         [('first', 1, 10), ('second', 2, 30)])

    def test_full_width_unterminated_csv_tail_is_omitted(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'frames.csv'
            path.write_bytes(b'frame,total_ms\r\n1,33.3\r\n2,4')
            rows, tail = frame_rows(path)
            self.assertTrue(tail)
            self.assertEqual(list(rows), [1])

    def test_short_unterminated_csv_tail_is_omitted(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'frames.csv'
            path.write_bytes(b'frame,total_ms\n1,33.3\n2')
            rows, tail = frame_rows(path)
            self.assertTrue(tail)
            self.assertEqual(list(rows), [1])

    def test_malformed_completed_csv_rows_are_not_discarded_as_tails(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'frames.csv'
            for malformed in (b'2\n', b'2,4,5\n', b'2,\n', b'2\n3,33\n'):
                with self.subTest(row=malformed):
                    path.write_bytes(b'frame,total_ms\n1,33.3\n' + malformed)
                    with self.assertRaisesRegex(ValueError, 'Malformed completed'):
                        frame_rows(path)

    def test_duplicate_frame_ids_are_rejected_not_silently_replaced(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'frames.csv'
            path.write_bytes(b'frame,total_ms\n1,33.3\n1,100\n')
            with self.assertRaisesRegex(ValueError, 'duplicate frame identity'):
                frame_rows(path)

    def test_report_joins_exact_frame_ids_despite_csv_order(self):
        with tempfile.TemporaryDirectory() as directory:
            case = Path(directory)
            (case / 'client.log').write_text(
                'world_frame_cpu_stage renderer_frame=18 app_frame=19 stage=command_finish '
                'thread_id=73 start_ns=100000000 end_ns=120000000 wall_ms=20 end=boundary\n',
                encoding='utf-8')
            (case / 'frame-timing.csv').write_bytes(b'frame,total_ms\n20,33\n19,70\n21,4')
            (case / 'gpu-profile.csv').write_bytes(b'frame,total_ms\n17,3\n18,8\n')
            report = analyze(case)
            self.assertEqual(report['stages'][0]['application']['total_ms'], '70')
            self.assertEqual(report['stages'][0]['gpu']['total_ms'], '8')
            self.assertEqual(report['partial_csv_tail_omitted'], {'application': True, 'gpu': False})
            json.dumps(report, allow_nan=False)


if __name__ == '__main__':
    unittest.main()
