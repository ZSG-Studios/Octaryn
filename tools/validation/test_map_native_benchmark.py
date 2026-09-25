"""Native benchmark resolution, safety and reflection quadrature contracts."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from capture_map_world import inspect_render_dimensions

ROOT = Path(__file__).resolve().parents[2]


class NativeBenchmark(unittest.TestCase):
    def test_native_extent(self):
        log = 'world_fsr2 version=2.2.1 mode=1 render=2560x1440 output=2560x1440'
        self.assertEqual(inspect_render_dimensions(log, 1, (2560, 1440)), [2560, 1440])

    def test_native_rejects_downscaling(self):
        log = 'world_fsr2 version=2.2.1 mode=1 render=853x480 output=2560x1440'
        with self.assertRaisesRegex(RuntimeError, 'below'):
            inspect_render_dimensions(log, 1, (2560, 1440))

    def test_upscaling_reported_honestly(self):
        log = 'world_fsr2 version=2.2.1 mode=5 render=853x480 output=2560x1440'
        self.assertEqual(inspect_render_dimensions(log, 5, (2560, 1440)), [853, 480])

    def test_missing_evidence_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'Missing'):
            inspect_render_dimensions('', 1, (2560, 1440))

    def test_uncapped_keeps_watchdog_heartbeat(self):
        source = (ROOT / 'tools/validation/capture_map_world.py').read_text()
        self.assertIn("    env['OCTARYN_CLIENT_LIVE_FRAME_TIMING'] = '1'", source)
        self.assertNotIn('if not args.uncapped_fps:', source)
        session = (ROOT / 'octaryn-client/Source/App/OpenWorld/MapWorldSession.cpp').read_text()
        self.assertIn("options.benchmark_hidden && uncapped_env && *uncapped_env=='1'", session)

    def test_temporal_reflections_are_default_with_explicit_fallback(self):
        source = (ROOT / 'octaryn-client/Source/Rendering/Hdr/MapReflections.cpp').read_text()
        self.assertIn("s.enabled=!(option && *option=='0')", source)

    def test_reflection_uses_selected_shading_extent(self):
        source = (ROOT / 'octaryn-client/Source/Rendering/Hdr/MapReflections.cpp').read_text()
        self.assertIn('reflection_extent(unsigned(r.render_width()),quality.divisor)', source)
        self.assertIn('reflection_extent(unsigned(r.render_height()),quality.divisor)', source)
        self.assertIn('scene_stable_for_reflections(r,s.scene_revision)', source)
        self.assertIn('if(!change.minor_build)stable=false', source)
        self.assertIn('s.scene_revision=r.scene_changes.revision()', source)
        composite = (ROOT / 'octaryn-client/Shaders/Hdr/Composite.slang').read_text()
        self.assertIn('temporalReflectionSurface', composite)
        self.assertIn('depthWeight=exp(', composite)

    def test_disocclusion_traces_each_direction_once(self):
        source = (ROOT / 'octaryn-client/Shaders/Hdr/MapReflectionTemporal.slang').read_text()
        self.assertIn('(sampleIndex+i)%sampleCount,sampleCount)', source)
        self.assertIn('#include "../Shadows/Reprojection.slang"', source)
        self.assertIn('shadow_history_position(oldWorld,world,voxel', source)
        for count, fresh in ((1, 1), (2, 1), (4, 1), (8, 2)):
            for start in range(count):
                rays = [(start + i) % count for i in range(fresh)]
                rays += [(start + i) % count for i in range(fresh, count)]
                self.assertEqual(sorted(rays), list(range(count)))


if __name__ == '__main__':
    unittest.main()
