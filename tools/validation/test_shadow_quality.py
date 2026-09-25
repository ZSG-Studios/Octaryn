"""Bounded shadow-tier and exact conservative early-out source/math contracts."""
from itertools import product
from pathlib import Path
import math
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
BACKEND = ROOT / "octaryn-client/Source/Rendering/RenderBackend"
SHADERS = ROOT / "octaryn-client/Shaders"


class ShadowQuality(unittest.TestCase):
    def test_bounded_policies_preserve_high(self):
        policy = (BACKEND / "ShadowQuality.h").read_text()
        for branch, values in (("case 0", "1,512,0"), ("case 1", "2,1024,1"),
                               ("case 3", "8,2048,2"), ("default", "4,1024,1")):
            self.assertIn(f"{branch}:return {{{values}}};", policy)

    def test_budget_bound_to_ray_shader_and_raster_filter(self):
        ray = (BACKEND / "RTShadowSystem.cpp").read_text()
        self.assertIn('c["shadowSamples"].setData(&shadow_samples', ray)
        self.assertIn('extent[1]*shadow_samples', ray)
        raster = (BACKEND / "ShadowFallbackSystem.cpp").read_text()
        self.assertIn('c["shadowFilterRadius"].setData(&filter_radius', raster)
        resolve = (SHADERS / "Shadows/ClipmapResolve.slang").read_text()
        self.assertIn('min(shadowFilterRadius,2u)', resolve)
        self.assertIn('1.0/float((radius*2+1)*(radius*2+1))', resolve)

    def test_stable_high_pattern_and_bounded_ultra_pattern(self):
        shader = (SHADERS / "RayTracing/Shadow.slang").read_text()
        self.assertIn('disk_offsets[4]={float2(0,0),float2(.75,0),'
                      'float2(-.375,.649519),float2(-.375,-.649519)}', shader)
        self.assertIn('tap<clamp(shadowSamples,1u,8u)', shader)
        pattern = re.search(r'ultra_offsets\[4\]=\{([^;]+)\};', shader)
        self.assertIsNotNone(pattern)
        offsets = re.findall(r'float2\(([-.\d]+),([-.\d]+)\)', pattern[1])
        self.assertEqual(len(offsets), 4)
        for x, y in offsets:
            self.assertLessEqual(math.hypot(float(x), float(y)), 1)

    def test_no_reintroduced_enclosure_leak_shortcuts(self):
        shader = (SHADERS / "RayTracing/Shadow.slang").read_text()
        query = (SHADERS / "RayTracing/WorldRayQuery.slang").read_text()
        self.assertIn('world_trace_ray(origin,sample_direction,raySettings.y,true,ignoredBlock)', shader)
        self.assertIn('const bool planeReceiver=mapReceiver || world_plant_voxel(voxel);', shader)
        self.assertIn('const float3 ignoredBlock=planeReceiver?', shader)
        self.assertIn('worldPosition-normal*0.001', shader)
        self.assertIn('bool world_face_contains_block(uint4 face,float3 ignoredBlock)', query)
        self.assertIn('if(world_face_contains_block(face,ignoredBlock))continue;', query)
        self.assertIn('conservative=min(conservative,sample_visibility)', shader)
        self.assertIn('if(conservative==0)break;', shader)
        self.assertNotIn('world_trace_ray(origin,sample_direction,shadowRange', shader)
        self.assertNotIn('result+=', shader)

    def test_early_occlusion_is_exact_for_all_supported_tiers(self):
        for count in (1, 2, 4, 8):
            for samples in product((0., .125, .5, 1.), repeat=count):
                visibility = 1.
                for sample in samples:
                    visibility = min(visibility, sample)
                    if visibility == 0:
                        break
                self.assertEqual(visibility, min(samples))


if __name__ == "__main__":
    unittest.main()
