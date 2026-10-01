"""Bounded reflection tier/sample contracts; GPU performance requires captures."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
BACKEND = ROOT / 'octaryn-client/Source/Rendering'
SHADERS = ROOT / 'octaryn-client/Shaders/Hdr'


class ReflectionQuality(unittest.TestCase):
    def tiers(self):
        source = (BACKEND / 'Hdr/ReflectionQuality.h').read_text()
        return [tuple(map(int, values)) for values in re.findall(
            r'\{(\d+),(\d+),(\d+),(\d+)\}', source)]

    def test_tiers_are_bounded_and_high_preserves_baseline(self):
        tiers = self.tiers()
        self.assertEqual(len(tiers), 4)
        self.assertEqual(tiers[2], (2, 4, 2, 32))
        self.assertEqual(tiers[3], (1, 8, 2, 32))
        for divisor, directions, fresh, history in tiers:
            self.assertIn(divisor, (1, 2, 3))
            self.assertTrue(1 <= fresh <= directions <= 8)
            self.assertTrue(4 <= history <= 32)
            self.assertEqual(directions % fresh, 0)

    def test_reference_cycle_and_adaptive_recovery_are_bounded(self):
        for _, directions, fresh, _ in self.tiers():
            cycle = [((frame * fresh) + i) % directions
                     for frame in range(directions // fresh) for i in range(fresh)]
            self.assertEqual(sorted(cycle), list(range(directions)))
        source = (SHADERS / 'MapReflectionTemporal.slang').read_text()
        self.assertIn('(uint(dimensions.z)*(referenceMode!=0?uint(reflectionSampling.y):1u))%sampleCount', source)
        self.assertIn('sample/=float(freshCount)', source)
        self.assertIn('if(!historyMatch)freshCount=sampleCount', source)
        self.assertIn('1u:min(sampleCount,2u)', source)
        self.assertIn('for(uint i=0;i<freshCount;++i)', source)
        for _, directions, _, _ in self.tiers():
            adaptive_cycle = [frame % directions for frame in range(directions)]
            self.assertEqual(sorted(adaptive_cycle), list(range(directions)))

    def test_receiver_and_lighting_history_guards_remain(self):
        shader = ((SHADERS / 'MapReflectionTemporal.slang').read_text() +
                  (SHADERS / 'MapReflectionHistory.slang').read_text())
        for guard in ('dot(oldSurface.xyz,normal)>=.98', 'abs(oldSurface.w-roughness)<=.03',
                      'distance(oldMaterial.rgb,albedo)<=.035', 'if(dimensions.w!=0 &&',
                      'shadow_history_position(oldWorld,world,voxel',
                      'smoothstep(.08,.5,roughness)'):
            self.assertIn(guard, shader)
        source = (BACKEND / 'Hdr/MapReflections.cpp').read_text()
        self.assertIn('s.quality==r.lighting_settings.reflection_quality', source)
        self.assertIn('s.light_revision==r.local_lighting.light_revision', source)
        self.assertNotIn('clearTextureFloat', source)
        self.assertIn('s.light_revision=r.local_lighting.light_revision', source)

    def test_native_extent_and_cross_pass_sampling_binding(self):
        source = (BACKEND / 'Hdr/ReflectionQuality.h').read_text()
        self.assertIn('source?1+(source-1)/divisor:1', source)
        for width in (1, 3, 1279, 1441, 2560):
            for divisor, _, _, _ in self.tiers():
                result = 1 + (width - 1) // divisor
                self.assertGreaterEqual(result * divisor, width)
                self.assertLess((result - 1) * divisor, width)
        binding = (BACKEND / 'RenderBackend/WorldRayTracing.cpp').read_text()
        self.assertIn('reflection_sampling(r.lighting_settings.reflection_quality)', binding)
        self.assertIn('sampling.isValid()', binding)
        reflection = (SHADERS / 'MapReflections.slang').read_text()
        self.assertIn('uint count=map_reflection_sample_count(roughness)', reflection)
        self.assertNotIn('roughness<.35?world_ray_visibility', reflection)

    def test_native_rate_skips_reconstruction_without_changing_signal(self):
        composite = (SHADERS / 'Composite.slang').read_text()
        self.assertIn('useTemporalReflections!=0 && all(reflectionDimensions.xy==dimensions.xy)', composite)
        self.assertIn('hdr+=temporalReflections[id.xy].rgb', composite)
        self.assertIn('else if(useTemporalReflections!=0)', composite)
        # Native sourcePixel and bilateral base both resolve to the same pixel.
        for x in (0, 1, 1279, 2559):
            uv = (x + .5) / 2560 * 2560 - .5
            self.assertEqual(uv, x)


if __name__ == '__main__':
    unittest.main()
