"""Passing process exit codes must not hide graphics validation failures."""
import unittest

from run_map_gpu_probe import inspect_log


class Evidence(unittest.TestCase):
    valid = ('map_material_gpu=passed\nmap_raster_gpu=passed\n'
             'map_pbr_environment_gpu=passed\n'
             'map_ray_enable_submitted\nmap_ray_enable_complete\n'
             'map_uploaded_mips_gpu=passed cached=0\n'
             'map_uploaded_mips_gpu=passed cached=1\n')

    def test_complete(self):
        inspect_log(self.valid)

    def test_missing(self):
        with self.assertRaises(RuntimeError):
            inspect_log('map_material_gpu=passed\n')

    def test_errors(self):
        for error in ('rhi_validation severity=error invalid index state',
                      'Validation Error: invalid state', 'VUID-123 failure',
                      'D3D12 ERROR invalid state', 'D3D12 CORRUPTION'):
            with self.subTest(error=error), self.assertRaises(RuntimeError):
                inspect_log(self.valid + error + '\n')


if __name__ == '__main__':
    unittest.main()
