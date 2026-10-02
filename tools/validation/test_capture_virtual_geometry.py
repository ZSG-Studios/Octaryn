"""Reject fallback draws, missing tiles and unfenced ray scene evidence."""
import argparse
from pathlib import Path
import tempfile
import unittest

from capture_virtual_geometry import (prepare_virtual_geometry, inspect_virtual_geometry,
                                      inspect_opaque_submissions, inspect_geometry_rays)

KEY = 'a'*64
INITIALIZED = (f'world_geometry_initialized mode=required asset={KEY} clusters=300 pages=20 root_pages=4 '
               'slots=20 pixels=1.000 root_ms=12.500 transparency=sorted_forward rt=virtual_geometry\n')
READY = f'world_geometry_ready mode=required asset={KEY} clusters=300 pages=20\n'
STREAM = (f'world_geometry_stream asset={KEY} frame=119 selected=91 resident_pages=8 pending_pages=2 '
          'gpu_bytes=1000000 uploaded_bytes=524288 feedback_overflow=0\n')
RAYS = (f'world_geometry_ray_ready asset={KEY} generation=1 clusters=150 batches=4 bytes=2097152 '
        'budget=33554432 offscreen=complete materials=authored error_pixels=1.000 requested_pixels=1.000 vertex_stride=104\n')
LOG = INITIALIZED+READY+STREAM


def args(**changes):
    fields = dict(virtual_geometry=None, geometry_pool_mib=384, geometry_pixels=1, geometry_occlusion='off',
                  performance_profile='custom', draw_mode='direct', lod_pixels=0, map_occlusion=False)
    return argparse.Namespace(**dict(fields, **changes))


class VirtualGeometryEvidenceTests(unittest.TestCase):
    def test_default_requires_real_paged_selection(self):
        inspect_opaque_submissions(LOG)
        for log in ('', INITIALIZED+READY, READY+STREAM, INITIALIZED+STREAM,
                    LOG.replace('selected=91', 'selected=0')):
            with self.assertRaises(RuntimeError):
                inspect_opaque_submissions(log)
        for legacy in ('map_draw forward=0 submitted=8 culled=2',
                       'map_draw forward=0 indirect=1 command_slots=12 cpu_submissions=1',
                       'map_draw forward=0 meshlet=1 meshlets=30 cpu_submissions=1'):
            with self.assertRaisesRegex(RuntimeError, 'Legacy'):
                inspect_opaque_submissions(LOG+legacy)

    def test_tiles_need_correct_identity_and_readiness(self):
        result = inspect_virtual_geometry(LOG+LOG.replace(KEY, 'b'*64))
        self.assertEqual(result['observed']['asset_count'], 2)
        self.assertEqual(result['observed']['selected_max'], 91)
        for log in (LOG+INITIALIZED.replace(KEY, 'b'*64), LOG+STREAM.replace(KEY, 'b'*64),
                    LOG+READY.replace('clusters=300', 'clusters=301')):
            with self.assertRaises(RuntimeError):
                inspect_virtual_geometry(log)

    def test_wrong_settings_bounds_and_failures_rejected(self):
        for log in (LOG.replace('slots=20', 'slots=6144'), LOG.replace('pixels=1.000', 'pixels=2.000'),
                    LOG.replace('selected=91', 'selected=301'), LOG.replace('resident_pages=8', 'resident_pages=3'),
                    LOG.replace('pending_pages=2', 'pending_pages=21'), LOG.replace('feedback_overflow=0', 'feedback_overflow=1'),
                    LOG+'world_geometry_failed reason=fixture\n'):
            with self.assertRaises(RuntimeError):
                inspect_virtual_geometry(log)

    def test_default_settings_are_required_without_an_explicit_cache(self):
        env = {}; evidence = prepare_virtual_geometry(args(), {}, env)
        self.assertTrue(evidence['enabled'])
        self.assertEqual(evidence['mode'], 'required')
        self.assertEqual(env['OCTARYN_CLIENT_VIRTUAL_GEOMETRY_OCCLUSION'], '0')
        prepare_virtual_geometry(args(), {'tiles': [[0]*6]}, {})
        for options in (dict(draw_mode='meshlet'), dict(lod_pixels=1), dict(map_occlusion=True),
                        dict(geometry_pool_mib=0), dict(geometry_pixels=float('nan')), dict(scene_stream=True)):
            with self.assertRaises(ValueError):
                prepare_virtual_geometry(args(**options), {}, {})

    def test_unused_explicit_cache_flag_cannot_claim_a_runtime_path(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)/'fixture.vgeom';cache.write_bytes(b'original geometry fixture')
            with self.assertRaisesRegex(ValueError, 'automatically'):
                prepare_virtual_geometry(args(virtual_geometry=cache), {}, {})

    def test_rays_require_published_bounded_scenes_for_each_asset(self):
        evidence = inspect_geometry_rays(LOG+RAYS.replace('error_pixels=1.000', 'error_pixels=4.000'))
        self.assertEqual(evidence['assets'][KEY]['error_pixels'], 4)
        self.assertEqual(evidence['assets'][KEY]['requested_pixels'], 1)
        inspect_geometry_rays(LOG+RAYS+(LOG+RAYS).replace(KEY, 'b'*64))
        for log in (LOG, LOG+RAYS.replace(KEY, 'b'*64), LOG+RAYS.replace('bytes=2097152', 'bytes=33554433'),
                    LOG+RAYS+LOG.replace(KEY, 'b'*64), LOG+RAYS.replace('offscreen=complete', 'offscreen=culled'),
                    LOG+RAYS.replace('error_pixels=1.000', 'error_pixels=-1.000'),
                    LOG+RAYS.replace(' error_pixels=1.000', ''), LOG+RAYS.replace('vertex_stride=104', 'vertex_stride=72')):
            with self.assertRaises(RuntimeError):
                inspect_geometry_rays(log)


if __name__ == '__main__':
    unittest.main()
