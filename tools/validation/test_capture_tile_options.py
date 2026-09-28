import copy
import csv
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest.mock import patch

from capture_tile_options import prepare_tile_options, inspect_tile_options


class TileCaptureOptionsTests(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.root = Path(folder.name)
        self.args = SimpleNamespace(tile_gpu_budget_mib=4096, require_all_tiles=True,
            settled_ready_window=[200, 3], frames=300, captures=1, capture_min_frame=180,
            stride=16, camera_motion=False)
        self.metadata = dict(enabled=True, tile_count=2, gpu_budget_mib=4096,
                             require_all_tiles=True, settled_ready_window=[200, 3])
        self.captures = [dict(lighting=dict(render_frame=190, ray_enabled=True))]
        self.log = ('tile_budget gpu_bytes=4294967296 upload_bytes=2097152\n'
                    'tile_published id=0 collision_ready=1 ray_ready=1\n'
                    'tile_published id=1 collision_ready=1 ray_ready=1\n'
                    'world_capture_tiles frame=190 resident=2 wanted=2 preparing=0 uploading=0 generation=2\n'
                    'tile_stream frame=300 resident=2 wanted=2 preparing=0 uploading=0 generation=2 reserved_bytes=0 retired_bytes=0 evicted=0 cancelled=0\n'
                    'tile_ray_schedule frame=300 polls=0 wait_fences=0 wait_allocations=0 operations=0 submissions=0 inflight=0 capacity=4 published=0 cancelled=0 work_tile=4294967295 work_step=6 work_ms=0 cpu_ms=0 soft_budget_ms=0.5 total_polls=2 total_wait_fences=1 total_wait_allocations=0 total_operations=4 total_submissions=2\n')
        self.camera = [dict(frame=210 + i, ready_frame=200 + i, phase='static',
                           eye_x=0, eye_y=3, eye_z=-20, yaw=.3, pitch=-.25) for i in range(3)]
        self.write('camera-motion.csv', self.camera)
        self.gpu = [dict(schema_version=4, frame=210+i, external_as_submissions=0, external_as_covered=1, external_as_ms=0, total_ms=5+i) for i in range(3)]
        self.write('gpu.csv', self.gpu)
        for name in ('frame-timing.csv', 'lighting.csv'):
            self.write(name, [dict(schema_version=3, frame=210+i, total_ms=2+i) for i in range(3)])

    def write(self, name, rows):
        with (self.root / name).open('w', newline='') as output:
            writer = csv.DictWriter(output, fieldnames=rows[0].keys())
            writer.writeheader()
            writer.writerows(rows)

    def inspect(self, log=None):
        return inspect_tile_options(self.root, self.metadata, self.log if log is None else log, self.captures)

    def test_explicit_budget_and_input_receipt(self):
        env = {}
        with patch('capture_tile_options.record_inputs') as record:
            result = prepare_tile_options(self.args, self.root/'map.json', {'tile_files':['a','b']}, self.root, env)
            record.assert_called_once()
        self.assertEqual(env['OCTARYN_CLIENT_TILE_GPU_BUDGET_MIB'], '4096')
        self.assertEqual(result['tile_count'], 2)

    def test_ordinary_monolithic_has_no_disk_or_env_side_effect(self):
        self.args.tile_gpu_budget_mib = None
        self.args.require_all_tiles = False
        self.args.settled_ready_window = None
        with patch('capture_tile_options.record_inputs') as record:
            env = {}
            metadata = prepare_tile_options(self.args, self.root/'map.json', {}, self.root, env)
            self.assertFalse(inspect_tile_options(self.root, metadata, '', [])['enabled'])
            record.assert_not_called()
        self.assertEqual(env, {})

    def test_invalid_budget_and_schedule(self):
        for key, value in [('tile_gpu_budget_mib', 63), ('tile_gpu_budget_mib', 32769),
                           ('captures', 0), ('camera_motion', True), ('settled_ready_window', [180, 3])]:
            args = copy.copy(self.args)
            setattr(args, key, value)
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                prepare_tile_options(args, self.root/'map.json', {'tile_files':['a','b']}, self.root, {})

    def test_exact_settled_window_after_full_capture(self):
        result = self.inspect()
        self.assertEqual(result['settled_window']['renderer_frames'], [210, 211, 212])
        self.assertEqual(result['all_manifest_ready_capture_frame'], 190)

    def test_partial_or_changed_residency_rejected(self):
        for bad in [self.log.replace('gpu_bytes=4294967296', 'gpu_bytes=2147483648'),
                    self.log.replace('id=1 collision_ready=1 ray_ready=1', 'id=1 collision_ready=0 ray_ready=1'),
                    self.log.replace('frame=190 resident=2', 'frame=190 resident=1'),
                    self.log + 'tile_upload bytes=256 budget=2097152 cpu_ms=0.1\n',
                    self.log + 'tile_stream frame=301 resident=2 wanted=2 preparing=0 uploading=0 generation=3\n',
                    self.log.replace('frame=300 resident=2 wanted=2 preparing=0 uploading=0 generation=2',
                                     'frame=300 resident=2 wanted=2 preparing=0 uploading=0 generation=3'),
                    self.log.replace('operations=0', 'operations=1')]:
            with self.subTest(log=bad), self.assertRaises(ValueError):
                self.inspect(bad)

    def test_window_missing_frame_camera_change_or_as_work_rejected(self):
        self.write('camera-motion.csv', self.camera[:2])
        with self.assertRaises(ValueError): self.inspect()
        self.camera[1]['eye_x'] = 1
        self.write('camera-motion.csv', self.camera)
        with self.assertRaises(ValueError): self.inspect()
        self.camera[1]['eye_x'] = 0
        self.write('camera-motion.csv', self.camera)
        self.write('gpu.csv', [dict(frame=210+i, external_as_submissions=int(i == 1)) for i in range(3)])
        with self.assertRaises(ValueError): self.inspect()



    def test_missing_malformed_or_absent_lifecycle_evidence_rejected(self):
        for text in [self.log.replace(' retired_bytes=0', ''),
                     self.log.replace('preparing=0', 'preparing=bad'),
                     self.log.replace('generation=2', 'generation=2 generation=2'),
                     '\n'.join(line for line in self.log.splitlines() if not line.startswith('tile_stream ')),
                     '\n'.join(line for line in self.log.splitlines() if not line.startswith('tile_ray_schedule '))]:
            with self.subTest(text=text), self.assertRaises(ValueError):
                self.inspect(text)

    def test_gpu_duplicate_schema_coverage_and_nonfinite_rejected(self):
        cases = [self.gpu + [self.gpu[0]]]
        for key, value in [('schema_version', 3), ('external_as_covered', 0),
                           ('total_ms', 'nan'), ('external_as_ms', .1)]:
            rows = copy.deepcopy(self.gpu)
            rows[1][key] = value
            cases.append(rows)
        rows = [{key: value for key, value in row.items() if key != 'external_as_covered'} for row in self.gpu]
        cases.append(rows)
        for rows in cases:
            self.write('gpu.csv', rows)
            with self.subTest(rows=rows), self.assertRaises(ValueError): self.inspect()

    def test_exact_window_metrics_and_unknown_os_budget(self):
        extra = dict(self.gpu[0], frame=1, total_ms=1000)
        self.write('gpu.csv', [extra] + self.gpu)
        result = self.inspect()
        self.assertEqual(result['settled_window']['measured']['gpu']['total_ms']['median'], 6)
        self.assertEqual(result['settled_window']['measured']['gpu']['total_ms']['samples'], 3)
        self.assertIsNone(result['os_budget_telemetry']['available'])
        observed = self.inspect(self.log + 'tile_resource_setup id=1 cpu_ms=1 os_budget_available=1\n')
        self.assertTrue(observed['os_budget_telemetry']['available'])

if __name__ == '__main__':
    unittest.main()
