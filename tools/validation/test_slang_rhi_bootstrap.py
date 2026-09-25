"""CPU-only dependency plan checks; never downloads, configures or builds."""
import importlib.util
from pathlib import Path
import unittest

SOURCE = Path(__file__).resolve().parents[1] / "build/slang-rhi.py"
SPEC = importlib.util.spec_from_file_location("slang_rhi_bootstrap", SOURCE)
BOOTSTRAP = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BOOTSTRAP)


class BootstrapPlanTests(unittest.TestCase):
    def test_native_backend_and_sdk_architecture(self):
        for system in ("linux", "macos"):
            for arch, archive_arch in (("x64", "x86_64"), ("arm64", "aarch64")):
                with self.subTest(system=system, arch=arch):
                    plan = BOOTSTRAP.build_plan(system, arch, "Release")
                    command = plan["configure"]
                    self.assertIn(f"{system}-{archive_arch}.tar.gz", plan["url"])
                    self.assertEqual(len(plan["sha256"]), 64)
                    self.assertIn("-DSLANG_RHI_ENABLE_D3D12=OFF", command)
                    self.assertIn("-DSLANG_RHI_FETCH_SLANG=OFF", command)
                    self.assertIn("-DSLANG_RHI_BUILD_SHARED=OFF", command)
                    enabled = "METAL" if system == "macos" else "VULKAN"
                    disabled = "VULKAN" if system == "macos" else "METAL"
                    self.assertIn(f"-DSLANG_RHI_ENABLE_{enabled}=ON", command)
                    self.assertIn(f"-DSLANG_RHI_ENABLE_{disabled}=OFF", command)
                    self.assertTrue(plan["build"].endswith(f"{system}-{arch}-Release"))

    def test_custom_sdk_with_spaces_and_config_are_single_arguments(self):
        sdk = SOURCE.parent / "scratch SDK with spaces"
        plan = BOOTSTRAP.build_plan("macos", "arm64", "Debug", sdk)
        self.assertIn(f"-DSLANG_RHI_SLANG_BINARY_DIR={sdk.resolve()}", plan["configure"])
        self.assertIn("-DCMAKE_OSX_ARCHITECTURES=arm64", plan["configure"])
        self.assertTrue(plan["build"].endswith("macos-arm64-Debug"))

    def test_windows_plan_uses_static_d3d12_vulkan_and_dxc(self):
        plan = BOOTSTRAP.build_plan("windows", "x64", "Release")
        command = plan["configure"]
        self.assertIn("-DSLANG_RHI_ENABLE_D3D12=ON", command)
        self.assertIn("-DSLANG_RHI_ENABLE_VULKAN=ON", command)
        self.assertIn("-DSLANG_RHI_FETCH_DXC=ON", command)
        self.assertIn("-DSLANG_RHI_FETCH_SLANG=OFF", command)
        self.assertIn("-DSLANG_RHI_BUILD_SHARED=OFF", command)
        self.assertIn("-DSLANG_RHI_ENABLE_METAL=OFF", command)
        self.assertNotIn("url", plan)
        self.assertTrue(plan["build"].endswith("windows-x64-Release"))

    def test_unknown_platform_or_arch_rejected(self):
        for system, arch in (("windows", "x86"), ("linux", "x86"), ("android", "arm64")):
            with self.assertRaises(ValueError):
                BOOTSTRAP.build_plan(system, arch, "Release")


if __name__ == "__main__":
    unittest.main()
