import unittest
import subprocess
import sys
from pathlib import Path
from capture_ray_counter_mode import apply_counter_mode, inspect_counter_mode, resolve_counter_mode


class CounterMode(unittest.TestCase):
    def test_only_qualified_windows_dx12_auto_removes_counters(self):
        for backend, platform, compiled in (('dx12','win32',False), ('vulkan','win32',True),
                                            ('metal','darwin',True), ('dx12','linux',True)):
            self.assertEqual(resolve_counter_mode('auto',False,backend,platform)['ray_counter_shader_resolved'], compiled)
            self.assertFalse(resolve_counter_mode('0',False,backend,platform)['ray_counter_shader_resolved'])
            self.assertTrue(resolve_counter_mode('auto',True,backend,platform)['ray_counter_shader_resolved'])
    def marker(self, request='auto', bit='0', collection='0'):
        return (f'ray_diagnostic_mode requested={request} resolved={bit} actual_compiled={bit} '
                f'collecting={collection} cache_variant=raycounters{bit} frozen=device')

    def test_production_and_legacy_quiet_are_distinct(self):
        production = resolve_counter_mode('auto', False)
        legacy = resolve_counter_mode('1', False)
        self.assertFalse(production['ray_counter_shader_resolved'])
        self.assertTrue(legacy['ray_counter_shader_resolved'])
        self.assertFalse(legacy['ray_counter_collection_requested'])
        self.assertFalse(inspect_counter_mode(self.marker(), production)['ray_counter_shader_actual'])
        self.assertTrue(inspect_counter_mode(self.marker('1', '1'), legacy)['ray_counter_shader_actual'])

    def test_collection_requires_compiled_counters(self):
        for option in ('auto', '1'):
            mode = resolve_counter_mode(option, True)
            self.assertTrue(inspect_counter_mode(self.marker(option, '1', '1'), mode)['ray_counter_shader_actual'])
        with self.assertRaises(ValueError):
            resolve_counter_mode('0', True)

    def test_auto_removes_inherited_control(self):
        env = {'OCTARYN_CLIENT_RAY_COUNTERS': '1'}
        apply_counter_mode('auto', False, env)
        self.assertNotIn('OCTARYN_CLIENT_RAY_COUNTERS', env)

    def test_missing_damaged_or_mismatched_activation_fails(self):
        mode = resolve_counter_mode('auto', False)
        for log in ('', 'prefix' + self.marker(), self.marker().replace('actual_compiled=0', 'actual_compiled=1'),
                    self.marker().replace('raycounters0', 'raycounters1'), self.marker('0'),
                    self.marker(collection='1'), self.marker() + '\n' + self.marker('1', '1')):
            with self.subTest(log=log), self.assertRaises(ValueError):
                inspect_counter_mode(log, mode)

    def test_capture_cli_rejects_collection_off_before_launch(self):
        root = Path(__file__).resolve().parents[2]
        result = subprocess.run([sys.executable, str(root / 'tools/validation/capture_map_world.py'),
            '--client-bundle-root', 'does-not-exist', '--evidence-root', 'does-not-exist',
            '--ray-counter-shader', '0', '--ray-diagnostics'], cwd=root, text=True, capture_output=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn('Ray diagnostic collection requires counters compiled in', result.stderr)


if __name__ == '__main__':
    unittest.main()
