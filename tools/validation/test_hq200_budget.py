"""HQ200 cannot pass by hiding slow frames, resolution or missing evidence."""
import unittest

from hq200_budget import CPU_BUDGETS, GPU_BUDGETS, SERVER_BUDGETS, evaluate


def fixture():
    cpu = {f: dict(schema_version=3, total_ms=4, display_width=2560, display_height=1440,
                   render_width=1280, render_height=720, upscaler_mode=6,
                   dynamic_resolution=1, ray_tracing_active=1) for f in range(200)}
    gpu = {f: dict(schema_version=4, total_gpu_ms=4, main_gpu_ms=4, external_as_ms=0,
                   external_as_submissions=0, external_as_kind=0, external_as_covered=1, opaque_ms=.5, forward_ms=.2,
                   sky_ms=.2, fsr_ms=.3, tonemap_ms=.05, ui_ms=.05, streaming_ms=.1) for f in cpu}
    lighting = {f: dict(schema_version=4, reflection_coverage_ms=0, reflection_screen_ms=0,
                        clouds_ms=.05, map_forward_ms=.15, reactive_copy_ms=.02, sun_trace_ms=.2, sun_filter_ms=.1, reflection_trace_ms=.5,
                        reflection_filter_ms=.1, reflection_classify_ms=.02,
                        local_cull_ms=.02, local_shade_ms=.1, composition_ms=.1,
                        dynamic_geometry_ms=.05, dynamic_motion_ms=.02) for f in cpu}
    return cpu, gpu, lighting


class BudgetChecks(unittest.TestCase):
    def test_allocations(self):
        self.assertAlmostEqual(sum(GPU_BUDGETS.values()), 5)
        self.assertAlmostEqual(sum(CPU_BUDGETS.values()), 3)
        self.assertAlmostEqual(sum(SERVER_BUDGETS.values()), 6)

    def test_render_pass_is_not_whole_contract(self):
        report = evaluate(*fixture())
        self.assertTrue(report['render_budget_passed'])
        self.assertFalse(report['contract_passed'])
        self.assertIn('quality', report['unqualified'])

    def test_single_streaming_hitch_is_not_discarded(self):
        cpu, gpu, light = fixture()
        cpu[100]['total_ms'] = 30
        report = evaluate(cpu, gpu, light)
        self.assertFalse(report['render_budget_passed'])
        self.assertEqual(report['checks']['frame_worst']['measured']['worst'], 30)

    def test_resolution_and_feature_cheats_fail(self):
        for key, value in [('render_width', 1279), ('render_height', 719),
                           ('ray_tracing_active', 0), ('upscaler_mode', 0),
                           ('dynamic_resolution', 0), ('display_width', 1920)]:
            cpu, gpu, light = fixture()
            cpu[70][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                evaluate(cpu, gpu, light)

    def test_missing_records_and_old_upload_scope_fail(self):
        cpu, gpu, light = fixture()
        del gpu[10]
        with self.assertRaises(ValueError): evaluate(cpu, gpu, light)
        cpu, gpu, light = fixture()
        gpu[10]['schema_version'] = 2
        with self.assertRaises(ValueError): evaluate(cpu, gpu, light)

    def test_sum_reflection_passes_per_frame(self):
        cpu, gpu, light = fixture()
        for row in light.values(): row['reflection_recovery_ms'] = .4
        report = evaluate(cpu, gpu, light)
        self.assertFalse(report['checks']['reflections']['passed'])
        self.assertAlmostEqual(report['checks']['reflections']['measured']['p99'], 1.02)

    def test_screen_and_coverage_costs_are_charged(self):
        cpu, gpu, light = fixture()
        for row in light.values():
            row['reflection_coverage_ms'] = .2
            row['reflection_screen_ms'] = .2
        result = evaluate(cpu, gpu, light)
        self.assertAlmostEqual(result['checks']['reflections']['measured']['p99'], 1.02)
        self.assertFalse(result['checks']['reflections']['passed'])

    def test_forward_costs_are_exclusive(self):
        cpu, gpu, light = fixture()
        for row in gpu.values(): row['forward_ms'] = 50
        result = evaluate(cpu, gpu, light)
        self.assertAlmostEqual(result['checks']['transparency']['measured']['p99'], .15)
        self.assertAlmostEqual(result['checks']['atmosphere']['measured']['p99'], .25)
        self.assertAlmostEqual(result['checks']['reconstruction']['measured']['p99'], .37)
        for row in light.values(): row['schema_version'] = 3
        result = evaluate(cpu, gpu, light)
        self.assertIn('mixed_forward', result['unqualified'])
        self.assertNotIn('transparency', result['checks'])
        self.assertFalse(result['render_budget_passed'])

    def test_floor_overrun_still_fails(self):
        cpu, gpu, light = fixture()
        for row in gpu.values(): row.update(total_gpu_ms=6, main_gpu_ms=6)
        report = evaluate(cpu, gpu, light)
        self.assertEqual(report['floor_gpu_overruns'], 200)
        self.assertFalse(report['render_budget_passed'])


    def test_independent_as_is_in_total_streaming_and_budget(self):
        cpu, gpu, light = fixture()
        for row in gpu.values():
            row.update(external_as_ms=.6, external_as_submissions=1, external_as_kind=1,
                       total_gpu_ms=4.6, streaming_ms=.7)
        report = evaluate(cpu, gpu, light)
        self.assertFalse(report['checks']['gpu_work']['passed'])
        self.assertFalse(report['checks']['streaming']['passed'])
        self.assertEqual(report['independent_as']['build_submissions'], 200)
        for field, value in [('total_gpu_ms',4), ('streaming_ms',.1), ('external_as_covered',0),
                             ('external_as_submissions',2), ('external_as_kind',0), ('external_as_ms',float('nan'))]:
            invalid = {key:dict(row) for key,row in gpu.items()}
            invalid[0][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                evaluate(cpu, invalid, light)

    def test_old_gpu_scope_cannot_qualify_streaming_budget(self):
        cpu, gpu, light = fixture()
        for row in gpu.values(): row['schema_version'] = 3
        report = evaluate(cpu, gpu, light)
        self.assertFalse(report['render_budget_passed'])
        self.assertIn('independent_as', report['unqualified'])
        cpu, gpu, light = fixture()
        del gpu[0]['external_as_ms']
        with self.assertRaisesRegex(ValueError,'missing independent AS'):
            evaluate(cpu, gpu, light)


if __name__ == '__main__':
    unittest.main()
