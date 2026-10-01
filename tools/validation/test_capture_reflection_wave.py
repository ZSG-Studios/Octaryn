import unittest
from capture_reflection_wave import resolve_wave_mode, apply_wave_mode, inspect_wave_mode


class WaveMode(unittest.TestCase):
    def marker(self,width=0,telemetry=0):
        return (f'reflection_wave_mode requested={width} forced={width} telemetry={telemetry} '
                'device_min=32 device_max=64 actual_observed=unknown frozen=device scope=fused_map_temporal')

    def test_defaults_do_not_claim_actual_width(self):
        for backend in ('dx12','vulkan','metal'):
            mode=resolve_wave_mode(0,backend,False)
            self.assertIsNone(inspect_wave_mode(self.marker(),mode)['reflection_wave_actual'])
        env={'OCTARYN_CLIENT_REFLECTION_WAVE_SIZE':'64'}
        apply_wave_mode(0,env)
        self.assertEqual(env['OCTARYN_CLIENT_REFLECTION_WAVE_SIZE'],'0')

    def test_forced_wave_requires_backend_capability_and_actual_pipeline(self):
        for width in (32,64):
            with self.assertRaises(ValueError):
                resolve_wave_mode(width,'vulkan',False)
            mode=resolve_wave_mode(width,'dx12',False)
            path=f'reflection_wave_path requested={width} map_only=1 temporal=1 queued=0 applied={width}'
            inspect_wave_mode(self.marker(width)+'\n'+path,mode)
            for invalid in ('',self.marker(width),self.marker(width)+'\n'+path.replace('map_only=1','map_only=0'),
                            self.marker(width)+'\n'+path.replace(f'applied={width}','applied=0')):
                with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                    inspect_wave_mode(invalid,mode)
        with self.assertRaises(ValueError):
            inspect_wave_mode(self.marker(64).replace('device_max=64','device_max=32'),resolve_wave_mode(64,'dx12',False))

    def test_requested_telemetry_cannot_be_silently_disabled(self):
        with self.assertRaises(ValueError):
            inspect_wave_mode(self.marker(),resolve_wave_mode(0,'dx12',True))


if __name__=='__main__':
    unittest.main()
