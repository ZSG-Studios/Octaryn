"""CPU energy sanity checks and production reflection source contracts (no GPU)."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
REFLECTION = ROOT / "octaryn-client/Shaders/Hdr/MapReflections.slang"
LUMA = (.2126, .7152, .0722)


def source_contract(source):
    source = re.sub(r"//[^\n]*|/\*.*?\*/", "", source, flags=re.S)
    assert "get_sky_color_lit" not in source
    assert re.search(r"if\(hit.hit\)radiance=map_reflected_radiance\([^;]+;\s*"
                     r"else radiance=diffuse_environment\(direction,sun.xyz,sky\)", source)
    assert re.search(r"if\([^;]+dot\(normal,view\)<=0\)return 0;", source)


class ReflectionEnergy(unittest.TestCase):
    def test_production_source_contract(self):
        source_contract(REFLECTION.read_text())

    def test_display_sky_negative_control(self):
        old = REFLECTION.read_text().replace(
            "diffuse_environment(direction,sun.xyz,sky)",
            "get_sky_color_lit(direction,sky.x,sky.z)")
        with self.assertRaises(AssertionError):
            source_contract(old)

    def test_known_daylight_energy(self):
        # Independent arithmetic sanity check of DiffuseEnvironment's calibrated
        # daylight case; source contracts above enforce the production call.
        baseline = tuple(c * 2.5 * .65 for c in (.235, .240, .245))
        luminance = sum(c * w for c, w in zip(baseline, LUMA))
        for zenith in (0, .25, .5, .75, 1):
            tint = tuple(1 + (c - 1) * zenith for c in (.92, .99, 1.08))
            colored = tuple(c * t for c, t in zip(baseline, tint))
            normalization = luminance / sum(c * w for c, w in zip(colored, LUMA))
            radiance = tuple(c * normalization for c in colored)
            self.assertAlmostEqual(sum(c * w for c, w in zip(radiance, LUMA)), luminance)
            self.assertLess(max(radiance), .5)
            # Normal-incidence smooth dielectric reflection: Fresnel F0=.04.
            self.assertLess(max(radiance) * .04, .02)
        # Saturated display blue is not calibrated incident radiance.
        old_blue = .999 / (1 - .999)
        self.assertGreater(old_blue * .04, 39)
        print("reflection_energy_cpu=passed daylight_luminance=%.6f old_display_blue=%.1f"
              % (luminance, old_blue))


if __name__ == "__main__":
    unittest.main()
