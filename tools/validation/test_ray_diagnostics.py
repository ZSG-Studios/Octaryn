"""Screen hits replace primary RT queries but retain secondary visibility work."""
import csv
from pathlib import Path
import tempfile
import unittest

from validate_ray_diagnostics import validate


class RayDiagnostics(unittest.TestCase):
    def check(self, expected_wave=0, require_reflections=True, **changes):
        row = dict(frame=1, schema_version=3, shadow_receivers=1, shadow_history_rejected=1,
                   shadow_history_accepted=0, shadow_queries=1, reflection_receivers=1,
                   reflection_history_rejected=0, reflection_queries=2, reflection_visibility_queries=3,
                   reflection_screen_attempts=2, reflection_screen_hits=1, reflection_screen_fallbacks=1)
        row.update(changes)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'ray.csv'
            with path.open('w', newline='') as stream:
                writer = csv.DictWriter(stream, row.keys())
                writer.writeheader()
                writer.writerow(row)
            return validate(path, require_secondary=require_reflections, expected_wave=expected_wave,
                            require_reflections=require_reflections)

    def test_explicitly_disabled_reflections_require_zero_actual_queries(self):
        fields = dict(reflection_queries=0, reflection_visibility_queries=0,
                      reflection_screen_attempts=0, reflection_screen_hits=0, reflection_screen_fallbacks=0)
        result = self.check(require_reflections=False, **fields)
        self.assertEqual(result['totals']['reflection_queries'], 0)
        with self.assertRaises(ValueError):
            self.check(**fields)
        with self.assertRaises(ValueError):
            self.check(require_reflections=False)

    def test_wave_observations_are_actual_not_requested_and_absence_unknown(self):
        fields = dict(schema_version=5, reflection_shaded_hits=3, reflection_unlit_hits=2,
                      reflection_material_skipped=2, reflection_emission_only=1,
                      reflection_wave_observations=64, reflection_wave_min=32, reflection_wave_max=32)
        result = self.check(expected_wave=32, **fields)
        self.assertEqual(result['reflection_wave']['minimum'],32)
        self.assertNotIn('reflection_wave_min', result['totals'])
        for changes in ({'reflection_wave_min':0}, {'reflection_wave_max':16},
                        {'reflection_wave_observations':0}, {'reflection_wave_min':31}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                self.check(**(fields | changes))
        with self.assertRaises(ValueError):
            self.check(expected_wave=64, **fields)
        empty = fields | dict(reflection_wave_observations=0, reflection_wave_min=0, reflection_wave_max=0)
        self.assertEqual(self.check(**empty)['reflection_wave']['status'],'unknown')
        with self.assertRaises(ValueError):
            self.check(expected_wave=32, **empty)
        self.assertEqual(self.check()['reflection_wave']['status'],'unknown')

    def test_screen_hit_still_has_visibility_query(self):
        result = self.check()
        self.assertEqual(result['totals']['reflection_screen_hits'], 1)
        self.assertFalse(result['timing_qualification'])

    def test_witness_savings_partition_logical_requests(self):
        fields = dict(schema_version=6, reflection_shaded_hits=3, reflection_unlit_hits=2,
                      reflection_material_skipped=2, reflection_emission_only=1,
                      reflection_wave_observations=0, reflection_wave_min=0, reflection_wave_max=0,
                      reflection_witness_logical=3, reflection_witness_tests=2,
                      reflection_witness_saved=1, reflection_witness_queries=2)
        result = self.check(**fields)
        self.assertEqual(result['totals']['reflection_witness_saved'], 1)
        for changes in ({'reflection_witness_logical':4}, {'reflection_witness_tests':0},
                        {'reflection_witness_tests':4}, {'reflection_witness_queries':4}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                self.check(**(fields | changes))

    def test_quad_current_work_partition(self):
        fields = dict(schema_version=7, reflection_shaded_hits=3, reflection_unlit_hits=2,
            reflection_material_skipped=2, reflection_emission_only=1,
            reflection_wave_observations=0, reflection_wave_min=0, reflection_wave_max=0,
            reflection_witness_logical=0, reflection_witness_tests=0,
            reflection_witness_saved=0, reflection_witness_queries=0,
            reflection_quad_cells=2, reflection_quad_source_rays=1,
            reflection_quad_reconstructed=1, reflection_quad_full_rate=0, reflection_quad_published=2)
        self.check(**fields)
        for changes in ({'reflection_quad_source_rays':3}, {'reflection_quad_published':3},
                        {'reflection_quad_full_rate':1}, {'reflection_quad_reconstructed':0}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                self.check(**(fields | changes))

    def test_invalid_partition_or_extra_visibility_fails(self):
        for changes in ({'reflection_screen_attempts': 3}, {'reflection_visibility_queries': 4}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                self.check(**changes)

    def test_historical_schema_remains_readable(self):
        for schema in (1, 2):
            self.check(schema_version=schema, reflection_visibility_queries=2,
                       reflection_screen_attempts=0, reflection_screen_hits=0,
                       reflection_screen_fallbacks=0)

    def test_deferred_material_counters_partition_actual_hits(self):
        fields = dict(schema_version=4, reflection_shaded_hits=3, reflection_unlit_hits=2,
                      reflection_material_skipped=2, reflection_emission_only=1)
        self.check(**fields)
        for field, value in (('reflection_shaded_hits', 4), ('reflection_unlit_hits', 4),
                             ('reflection_material_skipped', 3), ('reflection_emission_only', 3)):
            with self.subTest(field=field), self.assertRaises(ValueError):
                self.check(**(fields | {field: value}))


if __name__ == '__main__':
    unittest.main()
