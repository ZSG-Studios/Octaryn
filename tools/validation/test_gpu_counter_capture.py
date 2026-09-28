import copy
import json
from pathlib import Path
import tempfile
import unittest
from capture_gpu_counters import inspect_gpu_counters, resolve_gpu_counters, join_gpu_counter_frame
from types import SimpleNamespace


class GpuCounterCaptureTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.path = self.root / 'counter.jsonl'
        self.metadata = dict(enabled=True, sdk_version='4.4.0.5', dll=str(self.root / 'sdk.dll'),
                             sdk_api_version=[4, 4, 5, 0],
                             requested_frame=240, requested_names=None, output_dimensions=[2560, 1440], fixed_sampling=True)
        self.log = 'gpu_counter_profile status=complete frame=240 counters=1 passes=1'
        self.rows = [dict(event='configuration', schema=1, clock_mode='none', diagnostic_only=True),
                     dict(event='sdk', version=[4, 4, 5, 0], version_order='major,minor,build,update',
                          release_version='4.4.0.5', dll=self.metadata['dll']),
                     dict(event='device', name='fake'),
                     dict(event='counter', index=9, name='CSBusy', type='float64', description='Fake only',
                          uuid='00000000-0000-0000-0000-000000000009', sample_type=1),
                     dict(event='selection', passes=1, counters=1),
                     dict(event='enabled', ordinal=0, index=9, name='CSBusy'),
                     dict(event='submitted', renderer_frame=240, frame_fence=3, ready_frames=64,
                          requested_and_ray_scene_ready=True, scene_revision=1),
                     dict(event='result', index=9, name='CSBusy', type=0, value=12.5),
                     dict(event='complete', renderer_frame=240, timing_qualification=False, occupancy_claim=False),
                     dict(event='observation', renderer_frame=240, dimensions=[1280, 720, 2560, 1440],
                          camera=[0, 3, -20, .6, -.25, 1.047], fixed_sampling=True,
                          ready_frame=230, sampling_frame=230, delta_ms=1000 / 60)]

    def inspect(self, rows=None, log=None):
        self.path.write_text('\n'.join(json.dumps(row) for row in (rows or self.rows)))
        return inspect_gpu_counters(self.path, self.metadata, self.log if log is None else log)

    def test_typed_single_pass(self):
        result = self.inspect()
        self.assertTrue(result['enabled'])
        self.assertEqual(result['results'][0]['value'], 12.5)

    def test_rejects_wrong_pass_metadata_order_and_scope(self):
        for index, key, value in ((4, 'passes', 2), (5, 'ordinal', 1), (3, 'sample_type', 2),
                                  (6, 'ready_frames', 63), (8, 'occupancy_claim', True),
                                  (8, 'renderer_frame', 241), (7, 'name', 'Different')):
            with self.subTest(key=key):
                rows = copy.deepcopy(self.rows)
                rows[index][key] = value
                with self.assertRaises(ValueError):
                    self.inspect(rows)

    def test_rejects_write_failure_even_with_complete_record(self):
        with self.assertRaises(ValueError):
            self.inspect(log=self.log + '\nprofile_writer_failed capture_invalid=1')

    def test_rejects_partial_results_and_changed_sdk(self):
        with self.assertRaises(ValueError):
            self.inspect(self.rows[:-1])
        self.metadata['sdk_version'] = 'different'
        with self.assertRaises(ValueError):
            self.inspect()

    def test_api_order_is_not_release_version_order(self):
        self.inspect()
        rows = copy.deepcopy(self.rows)
        rows[1]['version'] = [4, 4, 0, 5]
        with self.assertRaisesRegex(ValueError, 'Actual SDK version'):
            self.inspect(rows)

    def test_explicit_counter_list_is_exact(self):
        self.metadata['requested_names'] = 'GPUTime'
        with self.assertRaises(ValueError):
            self.inspect()

    def test_disabled_does_not_read_output(self):
        self.assertEqual(inspect_gpu_counters(self.root / 'absent', dict(enabled=False), ''), dict(enabled=False))
        with self.assertRaises(ValueError):
            inspect_gpu_counters(self.path, dict(enabled=False), self.log)

    def test_only_dx12_fixed_1440_is_accepted(self):
        args = SimpleNamespace(gpu_counters=True, gpu_counter_names=None, gpu_counter_frame=240,
                               backend='dx12', ray_tracing='on', temporal_reflections='on', width=2560,
                               height=1440, frames=480, gameplay_route=None, camera_motion=False)
        resolve_gpu_counters(args)
        for key, value in (('backend', 'vulkan'), ('width', 640), ('gpu_counter_frame', 470),
                           ('gpu_counter_names', 'CSBusy,CSBusy')):
            with self.subTest(key=key):
                changed = copy.copy(args)
                setattr(changed, key, value)
                with self.assertRaises(ValueError):
                    resolve_gpu_counters(changed)

    def test_single_pass_allows_camera_motion(self):
        args = SimpleNamespace(gpu_counters=True, gpu_counter_names=None, gpu_counter_frame=600,
                               backend='dx12', ray_tracing='on', temporal_reflections='on', width=2560,
                               height=1440, frames=960, gameplay_route=None, camera_motion=True)
        resolve_gpu_counters(args)

    def test_exact_gpu_cpu_and_fence_frame_join(self):
        evidence = self.inspect()
        (self.root / 'gpu.csv').write_text('frame,width,height\n239,2560,1440\n240,2560,1440\n')
        (self.root / 'lighting.csv').write_text('frame,reflection_trace_ms\n240,4.5\n')
        evidence['frame_cpu_trace_requested'] = True
        trace = self.root / 'frame-retirement.csv'
        trace.write_text('renderer_frame,record,stage,source_frame,fence_value,success\n'
                         '240,frame,complete,0,0,1\n242,fence,fence_precheck,240,3,1\n')
        joined = join_gpu_counter_frame(self.root, evidence)
        self.assertEqual(joined['renderer_frame_join']['cpu_fence_retirement'][0]['renderer_frame'], '242')
        trace.write_text(trace.read_text().replace('240,3,1', '239,3,1'))
        with self.assertRaisesRegex(ValueError, 'fence retirement'):
            join_gpu_counter_frame(self.root, evidence)

    def test_requested_real_trace_missing_cannot_silently_skip_fence_join(self):
        evidence = self.inspect() | dict(frame_cpu_trace_requested=True)
        (self.root / 'gpu.csv').write_text('frame,width,height\n240,2560,1440\n')
        (self.root / 'lighting.csv').write_text('frame,reflection_trace_ms\n240,4.5\n')
        # The old guessed name must not satisfy the actual capture contract.
        (self.root / 'gpu.csv.retirement.csv').write_text('unused legacy filename\n')
        with self.assertRaisesRegex(ValueError, 'Requested CPU trace is missing: frame-retirement.csv'):
            join_gpu_counter_frame(self.root, evidence)


if __name__ == '__main__':
    unittest.main()
