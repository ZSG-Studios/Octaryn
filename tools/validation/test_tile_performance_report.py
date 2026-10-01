"""Keep startup, incomplete readiness and capture overhead visible in streaming evidence."""
import json
from pathlib import Path
import tempfile
import unittest

from tile_performance_report import frame_costs, parse_tile_work, hq200_activation


class TilePerformanceTests(unittest.TestCase):
    def test_authority_ready_is_not_full_visible_startup(self):
        partial = ('authoritative_player_ready session_elapsed_ms=1200\n'
                   'tile_published id=1 readiness_ms=22000 collision_ready=1 ray_ready=1\n'
                   'tile_stream resident=111 wanted=299 preparing=1 uploading=1\n')
        ready = parse_tile_work(partial, 299)['full_map_residency']
        self.assertFalse(ready['full_manifest_ready_observed'])
        completed = parse_tile_work(partial +
            'tile_published id=298 readiness_ms=22561.512 collision_ready=1 ray_ready=1\n'
            'tile_stream resident=299 wanted=299 preparing=0 uploading=0\n', 299)['full_map_residency']
        self.assertTrue(completed['full_manifest_ready_observed'])
        self.assertEqual(completed['first_complete']['request_span_lower_bound_ms'], 22561.512)
        self.assertIsNone(completed['process_launch_to_ready_ms'])
        self.assertFalse(parse_tile_work(partial, None)['full_map_residency']['full_manifest_ready_observed'])

    def test_slow_or_fast_full_map_does_not_decide_initial_playability(self):
        for ready_ms in (0, 500, 22561.512):
            with self.subTest(full_map_request_ms=ready_ms):
                log = ('authoritative_player_ready session_elapsed_ms=1200\n'
                       f'tile_published id=0 readiness_ms={ready_ms} collision_ready=1 ray_ready=1\n'
                       'tile_stream resident=1 wanted=1 preparing=0 uploading=0\n')
                result = parse_tile_work(log, 1)
                complete = result['full_map_residency']
                self.assertTrue(complete['full_manifest_ready_observed'])
                self.assertEqual(complete['request_span_lower_bound_ms'], ready_ms)
                self.assertNotIn('startup_budget_disposition', complete)
                playable = result['initial_playable_startup']
                self.assertFalse(playable['required_visible_collision_ready_observed'])
                self.assertIsNone(playable['process_launch_to_ready_ms'])
                self.assertEqual(playable['startup_budget_disposition'], 'unqualified')

    def test_hq200_activation_requires_actual_dimensions_rt_and_drs(self):
        row = dict(display_width=2560, display_height=1440, render_width=1280,
                   render_height=720, upscaler_mode=6, dynamic_resolution=1, ray_tracing_active=1)
        self.assertEqual(hq200_activation({0: row})['resolution_histogram'], {'1280x720': 1})
        for key, value in [('render_height', 719), ('render_width', 1921), ('display_width', 640),
                           ('upscaler_mode', 0), ('dynamic_resolution', 0), ('ray_tracing_active', 0)]:
            with self.subTest(key=key), self.assertRaises(ValueError):
                hq200_activation({0: dict(row, **{key: value})})
        with self.assertRaises(ValueError):
            hq200_activation({})

    def test_stage_boundary_and_unfinished_requests(self):
        log = ('tile_published id=1 readiness_ms=2001 deadline_missed=1\n'
               'tile_upload bytes=256 budget=512 cpu_ms=0.3 maximum_call_ms=0.1\n'
               'authoritative_player_ready eye=0,0,0 session_elapsed_ms=3000\n'
               'tile_published id=1 readiness_ms=8 deadline_missed=0\n'
               'tile_readiness_deadline id=2 phase=1 deadline_ms=2000 collision_ready=0\n'
               'tile_stream resident=1 wanted=3 preparing=1 uploading=1 cancelled=4 evicted=2\n')
        result = parse_tile_work(log)
        before, after = (result['stages'][key] for key in
                         ('before_authority_ready', 'after_authority_ready'))
        self.assertEqual(before['completed_request_ready_ms']['worst'], 2001)
        self.assertEqual(after['completed_request_ready_ms']['samples'], 1)
        self.assertEqual(after['completed_request_ready_ms']['median'], 8)
        self.assertEqual(result['deadline_miss_records']['after_authority_ready'], 1)
        self.assertEqual(result['final_tile_state']['preparing'], 1)
        self.assertEqual(result['final_tile_state']['cancelled'], 4)

    def test_zero_byte_pump_and_cumulative_max(self):
        result = parse_tile_work(
            'tile_upload bytes=0 budget=512 cpu_ms=17 maximum_call_ms=3 material_cpu_ms=17\n'
            'tile_upload bytes=256 budget=512 cpu_ms=1 maximum_call_ms=3 material_cpu_ms=17\n')
        stage = result['stages']['before_authority_ready']
        self.assertEqual(stage['tile_upload_cpu_ms']['worst'], 17)
        self.assertEqual(stage['tile_upload_cpu_ms']['samples'], 2)
        self.assertEqual(result['maximum_recorded_upload_call_ms'], 3)
        self.assertNotIn('material_cpu_ms', stage)
        self.assertFalse(result['authoritative_ready_observed'])

    def test_bad_cost_or_byte_budget_rejected(self):
        for line in ('tile_upload bytes=1024 budget=512 cpu_ms=1',
                     'tile_ray_pump cpu_ms=-2'):
            with self.subTest(line=line), self.assertRaises(ValueError):
                parse_tile_work(line)

    def test_owned_metrics_fail_closed_on_damage(self):
        valid = 'tile_upload bytes=256 budget=512 cpu_ms=0.3'
        damaged = [valid.replace('cpu_ms=0.3', 'cpu_ms=nan'),
                   valid.replace('cpu_ms=0.3', 'cpu_ms=inf'),
                   valid.replace('cpu_ms=0.3', 'cpu_ms=1e999'),
                   valid.replace('bytes=256 ', ''), valid.replace('cpu_ms=0.3', ''),
                   valid.replace('bytes=256', 'bytes=256rhi_validation severity=warning'),
                   'rhi_validation severity=warning ' + valid,
                   valid + ' rhi_validation severity=warning', valid + ' ' + valid,
                   valid + ' cpu_ms=5', valid.replace('tile_upload ', 'tile_uploadrhi '),
                   'tile_readiness_deadline id=12remote_session_window submitted=1 bytes=164\n'
                   '5 phase=0 deadline_ms=2000.000 collision_ready=0',
                   'tile_resource_setup id=4', 'tile_ray_pump id=4 cpu_ms=1',
                   'map_resource_allocation resources=3', 'map_ray_resources cpu_ms=1',
                   'map_compact_resource cpu_ms=1 bytes=NaN',
                   'tile_stream resident=1 wanted=2 preparing=0',
                   'tile_upload bytes=1.5 budget=512 cpu_ms=0.1']
        for line in damaged:
            with self.subTest(line=line), self.assertRaises(ValueError):
                parse_tile_work(line)

    def test_optional_counter_absence_is_unavailable_and_partial_loss_rejected(self):
        legacy = 'tile_upload bytes=256 budget=512 cpu_ms=0.3'
        result = parse_tile_work(legacy)
        self.assertIsNone(result['maximum_recorded_upload_call_ms'])
        self.assertIsNone(result['full_map_residency']['request_span_lower_bound_ms'])
        coverage = result['measurement_completeness']
        self.assertFalse(coverage['complete_event_coverage'])
        self.assertIn('maximum_call_ms', coverage['unavailable_optional_fields']['tile_upload'])
        self.assertIn('tile_ray_pump', coverage['unobserved_marker_types'])
        self.assertEqual(coverage['marker_records']['tile_upload'], 1)
        with self.assertRaisesRegex(ValueError, 'inconsistent fields'):
            parse_tile_work(legacy + ' maximum_call_ms=0.1\n' + legacy)

    def test_valid_extended_owner_records_preserve_real_zero_and_paths(self):
        log = ('tile_resource_setup id=1 cpu_ms=0.2 os_budget_available=1 reserved_bytes=4096\n'
               'tile_ray_pump id=1 cpu_ms=0 ready=1\n'
               'map_resource_allocation cpu_ms=0.1 maximum_call_ms=0.05 resources=3 cancelled=0\n'
               'map_ray_resources cpu_ms=0.1 triangles=10\n'
               'map_compact_resource cpu_ms=0 bytes=512\n'
               'tile_published id=1 file=tiles/room west.glb collision_ready=1 ray_ready=1 readiness_ms=0\n'
               'tile_stream resident=1 wanted=1 preparing=0 uploading=0 camera_x=-3.5\n')
        result = parse_tile_work(log, 1)
        self.assertEqual(result['full_map_residency']['request_span_lower_bound_ms'], 0)
        self.assertTrue(result['full_map_residency']['full_manifest_ready_observed'])
        self.assertEqual(result['final_tile_state']['camera_x'], -3.5)

    def test_frame_join_retains_spike_and_separates_capture(self):
        with tempfile.TemporaryDirectory() as directory:
            case = Path(directory)
            for filename in ('frame-timing.csv', 'gpu.csv', 'lighting.csv'):
                rows = ['schema_version,frame,total_ms,streaming_ms']
                rows += [f'3,{index},{80 if index == 1 else 5},{20 if index == 5 else 0}'
                         for index in range(7)]
                (case / filename).write_text('\n'.join(rows))
            (case / 'tile-route.csv').write_text(
                'frame,phase\n' + '\n'.join(f'{index},{"warmup" if index < 4 else "outbound"}'
                                            for index in range(7)))
            (case / 'frame.bmp.observation.json').write_text(json.dumps({'frame': 1}))
            result = frame_costs(case)
            self.assertEqual(result['matched_frames'], 7)
            self.assertEqual(result['all_frames_including_capture']['cpu']['total_ms']['worst'], 80)
            self.assertEqual(result['excluding_capture']['cpu']['total_ms']['worst'], 5)
            self.assertEqual(result['phases']['outbound']['gpu']['streaming_ms']['worst'], 20)
            self.assertEqual(result['excluded_capture_frames'], [0, 1, 2, 3])

    def test_missing_joined_frame_does_not_hide_cpu_tail(self):
        with tempfile.TemporaryDirectory() as directory:
            case = Path(directory)
            (case / 'frame-timing.csv').write_text('schema_version,frame,total_ms\n3,0,5\n3,1,90\n')
            (case / 'gpu.csv').write_text('schema_version,frame,total_ms\n3,0,4\n')
            result = frame_costs(case)
            self.assertEqual(result['missing_frames_by_group']['gpu'], [1])
            self.assertEqual(result['all_recorded_frames_by_group']['cpu']['total_ms']['worst'], 90)

    def test_missing_profiles_explicit(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual(frame_costs(Path(directory)), {'available': False})


if __name__ == '__main__':
    unittest.main()
