"""Do not confuse a selected mode, legacy draws, and executed paged geometry."""
import argparse
from pathlib import Path
import tempfile
import unittest

from capture_virtual_geometry import prepare_virtual_geometry, inspect_virtual_geometry, inspect_opaque_submissions


READY = ('world_geometry_ready mode=opt_in_monolithic clusters=300 pages=20 root_pages=4 '
         'slots=6144 pixels=1.000 root_ms=12.500 transparency=existing_forward rt=existing_full_detail\n')
STREAM = ('world_geometry_stream frame=119 selected=91 resident_pages=8 pending_pages=2 '
          'gpu_bytes=1000000 uploaded_bytes=524288 feedback_overflow=0\n')


def requested():
    return dict(enabled=True, pool_mib=384, pixels=1)


class VirtualGeometryEvidenceTests(unittest.TestCase):
    def test_existing_draw_paths_remain_required_without_opt_in(self):
        for line in ('map_draw forward=0 submitted=8 culled=2',
                     'map_draw forward=0 indirect=1 command_slots=12 cpu_submissions=1',
                     'map_draw forward=0 meshlet=1 meshlets=30 cpu_submissions=1'):
            inspect_opaque_submissions(line)
        for line in ('', 'map_draw forward=1 submitted=8 culled=2',
                     'map_draw forward=0 submitted=0 culled=2', READY+STREAM):
            with self.assertRaises(RuntimeError):
                inspect_opaque_submissions(line)

    def test_opt_in_requires_startup_and_actual_nonzero_selection(self):
        inspect_opaque_submissions(READY+STREAM, requested())
        for log in ('map_draw forward=0 submitted=8 culled=2', READY, STREAM,
                    READY+STREAM.replace('selected=91', 'selected=0'), READY+READY+STREAM):
            with self.assertRaises(RuntimeError):
                inspect_opaque_submissions(log, requested())
        evidence = inspect_virtual_geometry(READY+STREAM, requested())
        self.assertEqual(evidence['observed']['selected_max'], 91)
        self.assertEqual(evidence['observed']['root_pages'], 4)

    def test_wrong_settings_invalid_bounds_and_runtime_failures_rejected(self):
        for log in (READY.replace('slots=6144', 'slots=512')+STREAM,
                    READY.replace('pixels=1.000', 'pixels=2.000')+STREAM,
                    READY+STREAM.replace('selected=91', 'selected=301'),
                    READY+STREAM.replace('resident_pages=8', 'resident_pages=3'),
                    READY+STREAM.replace('pending_pages=2', 'pending_pages=6200'),
                    READY+STREAM+'world_geometry_failed reason=fixture\n'):
            with self.assertRaises(RuntimeError):
                inspect_virtual_geometry(log, requested())

    def test_explicit_options_record_cache_identity_and_detect_replacement(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)/'fixture.vgeom';cache.write_bytes(b'original geometry fixture')
            args = argparse.Namespace(virtual_geometry=cache, geometry_pool_mib=384, geometry_pixels=1,
                                      performance_profile='custom', draw_mode='direct', lod_pixels=0, map_occlusion=False)
            env = {};evidence = prepare_virtual_geometry(args, {'map': 'fixture.glb'}, env)
            self.assertEqual(env['OCTARYN_CLIENT_VIRTUAL_GEOMETRY'], str(cache.resolve()))
            self.assertEqual(env['OCTARYN_CLIENT_VIRTUAL_GEOMETRY_POOL_MIB'], '384')
            self.assertEqual(len(evidence['cache_sha256']), 64)
            inspect_virtual_geometry(READY+STREAM, evidence, verify_asset=True)
            cache.write_bytes(b'replacement geometry data')
            with self.assertRaisesRegex(RuntimeError, 'changed during'):
                inspect_virtual_geometry(READY+STREAM, evidence, verify_asset=True)
            with self.assertRaisesRegex(ValueError, 'monolithic'):
                prepare_virtual_geometry(args, {'tiles': [[0]*6]}, {})
            args.draw_mode = 'meshlet'
            with self.assertRaisesRegex(ValueError, 'cannot be combined'):
                prepare_virtual_geometry(args, {}, {})

    def test_settings_without_explicit_cache_cannot_enable_mode(self):
        args = argparse.Namespace(virtual_geometry=None, geometry_pool_mib=384, geometry_pixels=1)
        env = {};self.assertEqual(prepare_virtual_geometry(args, {}, env), {'enabled': False})
        self.assertEqual(env, {})
        args.geometry_pixels = 2
        with self.assertRaisesRegex(ValueError, 'require --virtual-geometry'):
            prepare_virtual_geometry(args, {}, env)


if __name__ == '__main__':
    unittest.main()
