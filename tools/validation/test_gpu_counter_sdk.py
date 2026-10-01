"""Portable acquisition integrity checks using a tiny archive, never a loaded DLL."""
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'build/support'))
from acquire_gpu_perf_api import acquire, api_version


class GpuCounterSdkTests(unittest.TestCase):
    def test_release_to_actual_api_argument_order(self):
        self.assertEqual(api_version('4.4.0.5'), [4, 4, 5, 0])
        self.assertEqual(api_version('1.2.3.4'), [1, 2, 4, 3])
        for value in ('4.4.5', '4.4.bad.5', '4.4.0.-5'):
            with self.assertRaises(ValueError):
                api_version(value)

    def fixture(self, unsafe=False):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        destination = root / 'build/dependencies/tools/gpu-perf-api/1.2.3.4'
        destination.mkdir(parents=True)
        archive = destination / 'sdk.zip'
        with zipfile.ZipFile(archive, 'w') as output:
            output.writestr('sdk/bin/GPUPerfAPIDX12-x64.dll', b'fake-not-loadable')
            output.writestr('sdk/include/gpu_performance_api/gpu_perf_api.h', b'fake-header')
            if unsafe:
                output.writestr('../escape', b'bad')
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        registry = root / 'cmake/Dependencies/DependencyRegistry.cmake'
        registry.parent.mkdir(parents=True)
        registry.write_text('octaryn_register_dependency(gpu_perf_api TAG1.2.3.4)'.replace('TAG1.2.3.4',
                            f'TAG 1.2.3.4 SOURCE_SUBDIR sdk URL https://invalid.test/sdk.zip URL_HASH SHA256={digest}'))
        return root, destination, archive

    def test_matched_headers_dll_and_repeat_verification(self):
        root, _, _ = self.fixture()
        first = acquire(root)
        self.assertEqual(first, acquire(root))
        self.assertFalse(first['loaded'])
        self.assertEqual(len(first['files']), 2)

    def test_archive_corruption_is_not_replaced(self):
        root, _, archive = self.fixture()
        archive.write_bytes(archive.read_bytes() + b'corrupt')
        with self.assertRaisesRegex(RuntimeError, 'archive checksum'):
            acquire(root)
        self.assertTrue(archive.read_bytes().endswith(b'corrupt'))

    def test_extracted_header_corruption_is_not_overwritten(self):
        root, _, _ = self.fixture()
        receipt = acquire(root)
        header = Path(receipt['sdk']) / 'include/gpu_performance_api/gpu_perf_api.h'
        header.write_bytes(b'changed')
        with self.assertRaisesRegex(RuntimeError, 'differs from pinned archive'):
            acquire(root)
        self.assertEqual(header.read_bytes(), b'changed')

    def test_archive_path_escape_is_rejected(self):
        root, _, _ = self.fixture(unsafe=True)
        with self.assertRaisesRegex(RuntimeError, 'Unsafe SDK archive path'):
            acquire(root)


if __name__ == '__main__':
    unittest.main()
