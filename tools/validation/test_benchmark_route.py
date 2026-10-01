"""No process launch: deterministic route resolution and camera evidence contracts."""
import json
from pathlib import Path
import tempfile
import unittest

from benchmark_route import float32, origin_arguments, resolve_origin, verify_origin


class BenchmarkOrigin(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.world = Path(self.temporary.name)
        self.player = dict(version=1, x=3.1705894, y=59.620003, z=-38.068565)
        self.save(self.player)

    def save(self, player):
        (self.world / 'player_1.json').write_text(json.dumps(player))

    def rows(self, origin, travel=160):
        x, y, z = origin['xyz']
        return [dict(camera_x=f'{x:.6f}', camera_y=f'{y:.6f}', camera_z=f'{value:.6f}')
                for value in (z, float32(z - float32(travel / 2)), float32(z - float32(travel)))]

    def test_saved_origin_is_independent_of_transient_runtime_pose(self):
        first = resolve_origin(self.world)
        (self.world / 'runtime').mkdir()
        for y in (59.620003, 59.65036, 60.0):
            (self.world / 'runtime/player_state.json').write_text(json.dumps(dict(playerY=y)))
            self.assertEqual(first, resolve_origin(self.world))
        self.assertEqual(first['xyz'], [float32(self.player['x']), float32(float32(self.player['y']) + 48), float32(self.player['z'])])
        self.assertAlmostEqual(first['xyz'][1], 107.620003, places=5)

    def test_explicit_origin_is_absolute_including_zero_and_negative(self):
        origin = resolve_origin(self.world, [-32, 0, -64])
        self.assertEqual(origin['xyz'], [-32, 0, -64])
        self.assertEqual(origin['source'], 'explicit_absolute_eye')
        self.assertEqual(verify_origin(self.rows(origin), origin, 160)['expected_endpoint'], [-32, 0, -224])

    def test_cli_round_trips_float32_without_pose_dependency(self):
        origin = resolve_origin(self.world)
        argv = origin_arguments(origin)
        self.assertEqual(argv[0], '--benchmark-route-origin')
        self.assertEqual([float32(float(value)) for value in argv[1:]], origin['xyz'])

    def test_no_saved_player_requires_explicit_origin(self):
        (self.world / 'player_1.json').unlink()
        with self.assertRaisesRegex(ValueError, 'requires saved'):
            resolve_origin(self.world)
        self.assertEqual(resolve_origin(self.world, [0, 80, 0])['xyz'], [0, 80, 0])

    def test_invalid_values_and_saved_version_are_rejected(self):
        for value in (float('nan'), float('inf'), -float('inf'), 1e39, '1', True):
            with self.subTest(value=value), self.assertRaises(ValueError):
                resolve_origin(self.world, [0, value, 0])
        for values in ([], [1, 2], [1, 2, 3, 4]):
            with self.assertRaises(ValueError):
                resolve_origin(self.world, values)
        self.save(dict(self.player, version=2))
        with self.assertRaises(ValueError):
            resolve_origin(self.world)

    def test_signed_block_range_and_route_endpoint_are_checked_before_launch(self):
        for coordinate in (1e20, -1e20, 2 ** 31 - 1, -(2 ** 31), 10 ** 1000):
            for axis in range(3):
                xyz = [0, 80, 0]
                xyz[axis] = coordinate
                with self.subTest(coordinate=coordinate, axis=axis), self.assertRaises(ValueError):
                    resolve_origin(self.world, xyz, 160, 32)
        # A valid start can leave the representable world during the native 3600 s route.
        xyz = [0, 80, -(2 ** 31) + 8192]
        self.assertEqual(resolve_origin(self.world, xyz, 160, 128)['xyz'], xyz)
        with self.assertRaisesRegex(ValueError, 'endpoint'):
            resolve_origin(self.world, xyz, 120 * 3600, 128)
        for radius in (4, 32, 128):
            with self.assertRaises(ValueError):
                resolve_origin(self.world, [2 ** 31 - 128, 80, 0], 160, radius)
        self.save(dict(self.player, x=1e20))
        with self.assertRaises(ValueError):
            resolve_origin(self.world, travel=160, radius=32)
        for travel in (float('inf'), 1e39, -1):
            with self.assertRaises(ValueError):
                resolve_origin(self.world, [0, 80, 0], travel, 32)

    def test_recorded_rounding_passes_but_three_centimetre_drift_fails(self):
        origin = resolve_origin(self.world)
        rows = self.rows(origin)
        self.assertEqual(verify_origin(rows, origin, 160)['xyz'], origin['xyz'])
        rows[0]['camera_y'] = '107.650360'
        with self.assertRaisesRegex(ValueError, 'Camera X/Y'):
            verify_origin(rows, origin, 160)

    def test_drift_in_any_row_or_wrong_endpoint_is_rejected(self):
        origin = resolve_origin(self.world)
        for index, key in ((0, 'camera_z'), (1, 'camera_x'), (1, 'camera_y'), (2, 'camera_z')):
            rows = self.rows(origin)
            rows[index][key] = str(float(rows[index][key]) + .01)
            with self.subTest(index=index, key=key), self.assertRaises(ValueError):
                verify_origin(rows, origin, 160)
        with self.assertRaises(ValueError):
            verify_origin([], origin, 160)


if __name__ == '__main__':
    unittest.main()
