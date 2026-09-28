"""Exercise production capture selection and executable activation evidence."""
import argparse
import unittest

from capture_render_options import (add_render_options, resolve_render_options,
    apply_render_options, render_option_evidence, inspect_render_options)


class RenderOptions(unittest.TestCase):
    def options(self, flags=(), platform='nt', backend='dx12', rays='on', queued=False):
        parser = argparse.ArgumentParser()
        add_render_options(parser)
        args = parser.parse_args(flags)
        args.backend, args.ray_tracing, args.rt_queued = backend, rays, queued
        args.temporal_reflections = 'on'
        resolve_render_options(args, platform)
        return args

    def test_defaults_are_platform_and_capability_scoped(self):
        for backend in ('dx12', 'vulkan'):
            args = self.options(backend=backend)
            self.assertTrue(args.rt_map_only and args.shadow_map_only and args.cloud_occlusion)
            self.assertTrue(args.rt_deferred_material)
        for platform, backend in (('posix', 'vulkan'), ('posix', 'metal'), ('nt', 'metal')):
            args = self.options(platform=platform, backend=backend)
            self.assertFalse(args.rt_map_only or args.shadow_map_only or args.cloud_occlusion)
        args = self.options(rays='off')
        self.assertTrue(args.cloud_occlusion)
        self.assertFalse(args.rt_map_only or args.shadow_map_only)
        self.assertFalse(self.options(queued=True).rt_map_only)

    def test_reference_controls_override_inherited_environment(self):
        args = self.options(('--rt-generic', '--shadow-generic', '--cloud-original', '--rt-eager-material'))
        env = {'OCTARYN_CLIENT_CLOUD_OCCLUSION': '1', 'OCTARYN_CLIENT_RT_DEFERRED_MATERIAL': '1'}
        apply_render_options(args, env)
        self.assertEqual(set(env.values()), {'0'})
        result = render_option_evidence(args)
        self.assertFalse(result['cloud_occlusion_requested'])
        self.assertIsNone(result['cloud_occlusion_active'])

    def test_explicit_unqualified_platform_requires_actual_opt_in(self):
        args = self.options(('--rt-map-only', '--shadow-map-only', '--cloud-occlusion'), platform='posix')
        self.assertTrue(args.rt_map_only and args.shadow_map_only and args.cloud_occlusion)

    def test_deferred_requires_compatible_traversal(self):
        self.assertTrue(self.options(('--rt-deferred-material',)).rt_deferred_material)
        for flags, overrides in ((('--rt-deferred-material', '--rt-generic'), {}),
                                 (('--rt-deferred-material',), {'queued': True}),
                                 (('--shadow-map-only',), {'rays': 'off'})):
            with self.subTest(flags=flags, overrides=overrides), self.assertRaises(ValueError):
                self.options(flags, **overrides)

    def test_runtime_markers_are_required_for_both_on_and_off(self):
        for flags, bit in (((), '1'), (('--rt-generic', '--shadow-generic', '--cloud-original'), '0')):
            args = self.options(flags)
            log = (f'clouds occlusion={bit} samples_unchanged=1\n'
                   f'rt_shadows map_only={bit} sampling_unchanged=1\n'
                   f'map_reflections map_only={bit} sampling_unchanged=1\n'
                   f'map_reflections deferred_material={bit} direct_gi=1 qualified_default={bit}\n')
            result = render_option_evidence(args)
            inspect_render_options(args, log, result)
            self.assertEqual(result['cloud_occlusion_active'], bit == '1')
            for invalid in ('', log.replace('clouds occlusion='+bit, 'clouds occlusion='+str(1-int(bit)))):
                with self.assertRaises(RuntimeError):
                    inspect_render_options(args, invalid, result)

    def test_retired_experiment_flags_are_rejected(self):
        import contextlib
        import io
        for flag in ('--rt-visibility-witness', '--rt-visibility-original', '--rt-rough-quad', '--rt-full-rate'):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                self.options((flag,))


if __name__ == '__main__':
    unittest.main()
