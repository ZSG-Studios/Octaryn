"""Exact-work shader contracts; no claim of GPU performance or image parity."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SHADERS = ROOT / 'octaryn-client/Shaders'


class MapLightingWork(unittest.TestCase):
    def test_reflected_shadow_skips_only_zero_sun(self):
        source = (SHADERS / 'Hdr/MapReflections.slang').read_text()
        self.assertNotIn('roughness<.35?world_ray_visibility', source)
        self.assertRegex(source, r'sun\.w!=0 && dot\(hit.normal,sun.xyz\)>0\?\s*world_ray_visibility')
        self.assertIn('bt_reflected_sun_brdf(cachedDiffuse,hit.albedo.rgb,metallic,roughness,hit.normal,view,sun.xyz)*sun.w*visibility', source)

    def test_early_depth_only_on_non_depth_writing_forward(self):
        source = (SHADERS / 'Map/WorldMap.slang').read_text()
        self.assertEqual(source.count('[earlydepthstencil]'), 1)
        self.assertRegex(source, r'\[earlydepthstencil\]\s*float4 forward_main')
        renderer = (ROOT / 'octaryn-client/Source/MapWorld/MapRenderer.cpp').read_text()
        self.assertRegex(renderer, r'depthWriteEnable=false;[\s\S]*map.forward_pipeline')

    def test_sky_opaque_terminates_but_blends_do_not_commit(self):
        source = (SHADERS / 'RayTracing/WorldSkyVisibility.slang').read_text()
        self.assertIn('query.TraceRayInline(rayScene,RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,255,ray)', source)
        self.assertNotIn('CommitNonOpaqueTriangleHit', source)
        self.assertIn('transmission*=1-saturate(alpha)', source)
        self.assertIn('query.CommittedStatus()==COMMITTED_TRIANGLE_HIT?0:transmission', source)


if __name__ == '__main__':
    unittest.main()
