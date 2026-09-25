"""CPU-only regression for the pinned Vulkan indexed BLAS count repair."""
import hashlib
import importlib.util
from pathlib import Path
import subprocess
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('bootstrap', ROOT / 'tools/build/slang-rhi.py')
BOOTSTRAP = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BOOTSTRAP)
PATCH = 'slang-rhi-vulkan-indexed-primitive-count.patch'
SOURCE = 'src/vulkan/vk-acceleration-structure.cpp'


class IndexedTriangleTests(unittest.TestCase):
    def test_patch_applies_idempotently_and_preserves_unrelated_files(self):
        upstream = subprocess.check_output(
            ['git', '-C', str(ROOT / 'build/dependencies/slang-rhi'), 'show', f'HEAD:{SOURCE}'])
        with tempfile.TemporaryDirectory() as directory:
            checkout = Path(directory)
            target = checkout / SOURCE
            target.parent.mkdir(parents=True)
            target.write_bytes(upstream)
            subprocess.run(['git', 'init', '-q', directory], check=True)
            subprocess.run(['git', '-C', directory, 'add', '.'], check=True)
            subprocess.run(['git', '-C', directory, '-c', 'user.name=Test',
                            '-c', 'user.email=test@example.invalid', 'commit', '-qm', 'fixture'], check=True)
            sentinel = checkout / 'untracked-owner-content'
            sentinel.write_text('preserve')
            registry = BOOTSTRAP.PATCHES
            try:
                BOOTSTRAP.PATCHES = (PATCH,)
                BOOTSTRAP.patch_checkout(checkout)
                first = target.read_bytes()
                BOOTSTRAP.patch_checkout(checkout)
                self.assertEqual(first, target.read_bytes())
                # Fresh Windows CRLF checkout must apply the identical registered patch.
                target.write_bytes(upstream.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'))
                BOOTSTRAP.patch_checkout(checkout)
                BOOTSTRAP.patch_checkout(checkout)
            finally:
                BOOTSTRAP.PATCHES = registry
            self.assertEqual(sentinel.read_text(), 'preserve')
            text = target.read_text()
            selector = '(triangles.indexBuffer ? triangles.indexCount : triangles.vertexCount) / 3'
            self.assertIn(f'primitiveCounts[i] = {selector};', text)
            self.assertNotIn('max(triangles.vertexCount, triangles.indexCount) / 3', text)
            # Independent expected indexed/nonindexed counts for padded shared vertices.
            for indexed, vertices, indices, expected in (
                    (True, 4096, 3, 1), (True, 6, 12, 4), (False, 9, 0, 3)):
                self.assertEqual((indices if indexed else vertices) // 3, expected)

    def test_patch_receipt_matches_cmake_and_rejects_stale_hash(self):
        entries = ''.join(f'{name}:{hashlib.sha256((ROOT / "tools/build/patches" / name).read_text().encode()).hexdigest()}\n'
                          for name in BOOTSTRAP.PATCHES)
        self.assertEqual(BOOTSTRAP.patch_hash(), hashlib.sha256(entries.encode()).hexdigest())
        cmake = ROOT / 'cmake/Dependencies/SlangRhiPatchReceipt.cmake'
        executable = shutil.which('cmake') or 'C:/Program Files/CMake/bin/cmake.exe'
        command = [executable, f'-DOCTARYN_WORKSPACE_ROOT_DIR={ROOT.as_posix()}']
        subprocess.run(command + [f'-DOCTARYN_SLANG_RHI_BUILT_PATCH_HASH={BOOTSTRAP.patch_hash()}',
                                  '-P', str(cmake)], check=True)
        result = subprocess.run(command + ['-DOCTARYN_SLANG_RHI_BUILT_PATCH_HASH=stale', '-P', str(cmake)],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('patch receipt is stale', result.stderr)


if __name__ == '__main__':
    unittest.main()
